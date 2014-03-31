/*
 * Copyright 2000 by Hans Reiser, licensing governed by reiserfs/README
 */

#include <linux/time.h>
#include "reiserfs.h"
#include <linux/errno.h>
#include <linux/buffer_head.h>
#include <linux/kernel.h>
#include <linux/pagemap.h>
#include <linux/vmalloc.h>
#include <linux/quotaops.h>
#include <linux/seq_file.h>

#define PREALLOCATION_SIZE 9


#define SB_ALLOC_OPTS(s) (REISERFS_SB(s)->s_alloc_options.bits)

#define  _ALLOC_concentrating_formatted_nodes 0
#define  _ALLOC_displacing_large_files 1
#define  _ALLOC_displacing_new_packing_localities 2
#define  _ALLOC_old_hashed_relocation 3
#define  _ALLOC_new_hashed_relocation 4
#define  _ALLOC_skip_busy 5
#define  _ALLOC_displace_based_on_dirid 6
#define  _ALLOC_hashed_formatted_nodes 7
#define  _ALLOC_old_way 8
#define  _ALLOC_hundredth_slices 9
#define  _ALLOC_dirid_groups 10
#define  _ALLOC_oid_groups 11
#define  _ALLOC_packing_groups 12

#define  concentrating_formatted_nodes(s)	test_bit(_ALLOC_concentrating_formatted_nodes, &SB_ALLOC_OPTS(s))
#define  displacing_large_files(s)		test_bit(_ALLOC_displacing_large_files, &SB_ALLOC_OPTS(s))
#define  displacing_new_packing_localities(s)	test_bit(_ALLOC_displacing_new_packing_localities, &SB_ALLOC_OPTS(s))

#define SET_OPTION(optname) \
   do { \
	reiserfs_info(s, "block allocator option \"%s\" is set", #optname); \
	set_bit(_ALLOC_ ## optname , &SB_ALLOC_OPTS(s)); \
    } while(0)
#define TEST_OPTION(optname, s) \
    test_bit(_ALLOC_ ## optname , &SB_ALLOC_OPTS(s))

static inline void get_bit_address(struct super_block *s,
				   b_blocknr_t block,
				   unsigned int *bmap_nr,
				   unsigned int *offset)
{
	*bmap_nr = block >> (s->s_blocksize_bits + 3);
	
	*offset = block & ((s->s_blocksize << 3) - 1);
}

int is_reusable(struct super_block *s, b_blocknr_t block, int bit_value)
{
	unsigned int bmap, offset;
	unsigned int bmap_count = reiserfs_bmap_count(s);

	if (block == 0 || block >= SB_BLOCK_COUNT(s)) {
		reiserfs_error(s, "vs-4010",
			       "block number is out of range %lu (%u)",
			       block, SB_BLOCK_COUNT(s));
		return 0;
	}

	get_bit_address(s, block, &bmap, &offset);

	if (unlikely(test_bit(REISERFS_OLD_FORMAT,
			      &(REISERFS_SB(s)->s_properties)))) {
		b_blocknr_t bmap1 = REISERFS_SB(s)->s_sbh->b_blocknr + 1;
		if (block >= bmap1 &&
		    block <= bmap1 + bmap_count) {
			reiserfs_error(s, "vs-4019", "bitmap block %lu(%u) "
				       "can't be freed or reused",
				       block, bmap_count);
			return 0;
		}
	} else {
		if (offset == 0) {
			reiserfs_error(s, "vs-4020", "bitmap block %lu(%u) "
				       "can't be freed or reused",
				       block, bmap_count);
			return 0;
		}
	}

	if (bmap >= bmap_count) {
		reiserfs_error(s, "vs-4030", "bitmap for requested block "
			       "is out of range: block=%lu, bitmap_nr=%u",
			       block, bmap);
		return 0;
	}

	if (bit_value == 0 && block == SB_ROOT_BLOCK(s)) {
		reiserfs_error(s, "vs-4050", "this is root block (%u), "
			       "it must be busy", SB_ROOT_BLOCK(s));
		return 0;
	}

	return 1;
}

static inline int is_block_in_journal(struct super_block *s, unsigned int bmap,
				      int off, int *next)
{
	b_blocknr_t tmp;

	if (reiserfs_in_journal(s, bmap, off, 1, &tmp)) {
		if (tmp) {	
			*next = tmp;
			PROC_INFO_INC(s, scan_bitmap.in_journal_hint);
		} else {
			(*next) = off + 1;	
			PROC_INFO_INC(s, scan_bitmap.in_journal_nohint);
		}
		PROC_INFO_INC(s, scan_bitmap.retry);
		return 1;
	}
	return 0;
}

static int scan_bitmap_block(struct reiserfs_transaction_handle *th,
			     unsigned int bmap_n, int *beg, int boundary,
			     int min, int max, int unfm)
{
	struct super_block *s = th->t_super;
	struct reiserfs_bitmap_info *bi = &SB_AP_BITMAP(s)[bmap_n];
	struct buffer_head *bh;
	int end, next;
	int org = *beg;

	BUG_ON(!th->t_trans_id);

	RFALSE(bmap_n >= reiserfs_bmap_count(s), "Bitmap %u is out of "
	       "range (0..%u)", bmap_n, reiserfs_bmap_count(s) - 1);
	PROC_INFO_INC(s, scan_bitmap.bmap);

	if (!bi) {
		reiserfs_error(s, "jdm-4055", "NULL bitmap info pointer "
			       "for bitmap %d", bmap_n);
		return 0;
	}

	bh = reiserfs_read_bitmap_block(s, bmap_n);
	if (bh == NULL)
		return 0;

	while (1) {
	      cont:
		if (bi->free_count < min) {
			brelse(bh);
			return 0;	
		}

		
		*beg = reiserfs_find_next_zero_le_bit
		    ((unsigned long *)(bh->b_data), boundary, *beg);

		if (*beg + min > boundary) {	
			brelse(bh);
			return 0;
		}

		if (unfm && is_block_in_journal(s, bmap_n, *beg, beg))
			continue;
		
		for (end = *beg + 1;; end++) {
			if (end >= *beg + max || end >= boundary
			    || reiserfs_test_le_bit(end, bh->b_data)) {
				next = end;
				break;
			}
			if (unfm && is_block_in_journal(s, bmap_n, end, &next))
				break;
		}

		if (end - *beg >= min) {	
			int i;
			reiserfs_prepare_for_journal(s, bh, 1);
			
			for (i = *beg; i < end; i++) {
				
				if (reiserfs_test_and_set_le_bit
				    (i, bh->b_data)) {
					PROC_INFO_INC(s, scan_bitmap.stolen);
					if (i >= *beg + min) {	
						end = i;
						break;
					}
					
					while (--i >= *beg)
						reiserfs_clear_le_bit
						    (i, bh->b_data);
					reiserfs_restore_prepared_buffer(s, bh);
					*beg = org;
					
					goto cont;
				}
			}
			bi->free_count -= (end - *beg);
			journal_mark_dirty(th, s, bh);
			brelse(bh);

			
			reiserfs_prepare_for_journal(s, SB_BUFFER_WITH_SB(s),
						     1);
			PUT_SB_FREE_BLOCKS(s, SB_FREE_BLOCKS(s) - (end - *beg));
			journal_mark_dirty(th, s, SB_BUFFER_WITH_SB(s));

			return end - (*beg);
		} else {
			*beg = next;
		}
	}
}

static int bmap_hash_id(struct super_block *s, u32 id)
{
	char *hash_in = NULL;
	unsigned long hash;
	unsigned bm;

	if (id <= 2) {
		bm = 1;
	} else {
		hash_in = (char *)(&id);
		hash = keyed_hash(hash_in, 4);
		bm = hash % reiserfs_bmap_count(s);
		if (!bm)
			bm = 1;
	}
	
	if (bm >= reiserfs_bmap_count(s))
		bm = 0;
	return bm;
}

static inline int block_group_used(struct super_block *s, u32 id)
{
	int bm = bmap_hash_id(s, id);
	struct reiserfs_bitmap_info *info = &SB_AP_BITMAP(s)[bm];

	if (info->free_count == UINT_MAX) {
		struct buffer_head *bh = reiserfs_read_bitmap_block(s, bm);
		brelse(bh);
	}

	if (info->free_count > ((s->s_blocksize << 3) * 60 / 100)) {
		return 0;
	}
	return 1;
}

__le32 reiserfs_choose_packing(struct inode * dir)
{
	__le32 packing;
	if (TEST_OPTION(packing_groups, dir->i_sb)) {
		u32 parent_dir = le32_to_cpu(INODE_PKEY(dir)->k_dir_id);
		if (parent_dir == 1 || block_group_used(dir->i_sb, parent_dir))
			packing = INODE_PKEY(dir)->k_objectid;
		else
			packing = INODE_PKEY(dir)->k_dir_id;
	} else
		packing = INODE_PKEY(dir)->k_objectid;
	return packing;
}

static int scan_bitmap(struct reiserfs_transaction_handle *th,
		       b_blocknr_t * start, b_blocknr_t finish,
		       int min, int max, int unfm, sector_t file_block)
{
	int nr_allocated = 0;
	struct super_block *s = th->t_super;

	unsigned int bm, off;
	unsigned int end_bm, end_off;
	unsigned int off_max = s->s_blocksize << 3;

	BUG_ON(!th->t_trans_id);

	PROC_INFO_INC(s, scan_bitmap.call);
	if (SB_FREE_BLOCKS(s) <= 0)
		return 0;	

	get_bit_address(s, *start, &bm, &off);
	get_bit_address(s, finish, &end_bm, &end_off);
	if (bm > reiserfs_bmap_count(s))
		return 0;
	if (end_bm > reiserfs_bmap_count(s))
		end_bm = reiserfs_bmap_count(s);

	if (TEST_OPTION(skip_busy, s)
	    && SB_FREE_BLOCKS(s) > SB_BLOCK_COUNT(s) / 20) {
		for (; bm < end_bm; bm++, off = 0) {
			if ((off && (!unfm || (file_block != 0)))
			    || SB_AP_BITMAP(s)[bm].free_count >
			    (s->s_blocksize << 3) / 10)
				nr_allocated =
				    scan_bitmap_block(th, bm, &off, off_max,
						      min, max, unfm);
			if (nr_allocated)
				goto ret;
		}
		
		get_bit_address(s, *start, &bm, &off);
	}

	for (; bm < end_bm; bm++, off = 0) {
		nr_allocated =
		    scan_bitmap_block(th, bm, &off, off_max, min, max, unfm);
		if (nr_allocated)
			goto ret;
	}

	nr_allocated =
	    scan_bitmap_block(th, bm, &off, end_off + 1, min, max, unfm);

      ret:
	*start = bm * off_max + off;
	return nr_allocated;

}

static void _reiserfs_free_block(struct reiserfs_transaction_handle *th,
				 struct inode *inode, b_blocknr_t block,
				 int for_unformatted)
{
	struct super_block *s = th->t_super;
	struct reiserfs_super_block *rs;
	struct buffer_head *sbh, *bmbh;
	struct reiserfs_bitmap_info *apbi;
	unsigned int nr, offset;

	BUG_ON(!th->t_trans_id);

	PROC_INFO_INC(s, free_block);

	rs = SB_DISK_SUPER_BLOCK(s);
	sbh = SB_BUFFER_WITH_SB(s);
	apbi = SB_AP_BITMAP(s);

	get_bit_address(s, block, &nr, &offset);

	if (nr >= reiserfs_bmap_count(s)) {
		reiserfs_error(s, "vs-4075", "block %lu is out of range",
			       block);
		return;
	}

	bmbh = reiserfs_read_bitmap_block(s, nr);
	if (!bmbh)
		return;

	reiserfs_prepare_for_journal(s, bmbh, 1);

	
	if (!reiserfs_test_and_clear_le_bit(offset, bmbh->b_data)) {
		reiserfs_error(s, "vs-4080",
			       "block %lu: bit already cleared", block);
	}
	apbi[nr].free_count++;
	journal_mark_dirty(th, s, bmbh);
	brelse(bmbh);

	reiserfs_prepare_for_journal(s, sbh, 1);
	
	set_sb_free_blocks(rs, sb_free_blocks(rs) + 1);

	journal_mark_dirty(th, s, sbh);
	if (for_unformatted)
		dquot_free_block_nodirty(inode, 1);
}

void reiserfs_free_block(struct reiserfs_transaction_handle *th,
			 struct inode *inode, b_blocknr_t block,
			 int for_unformatted)
{
	struct super_block *s = th->t_super;
	BUG_ON(!th->t_trans_id);

	RFALSE(!s, "vs-4061: trying to free block on nonexistent device");
	if (!is_reusable(s, block, 1))
		return;

	if (block > sb_block_count(REISERFS_SB(s)->s_rs)) {
		reiserfs_error(th->t_super, "bitmap-4072",
			       "Trying to free block outside file system "
			       "boundaries (%lu > %lu)",
			       block, sb_block_count(REISERFS_SB(s)->s_rs));
		return;
	}
	
	journal_mark_freed(th, s, block);
	_reiserfs_free_block(th, inode, block, for_unformatted);
}

static void reiserfs_free_prealloc_block(struct reiserfs_transaction_handle *th,
					 struct inode *inode, b_blocknr_t block)
{
	BUG_ON(!th->t_trans_id);
	RFALSE(!th->t_super,
	       "vs-4060: trying to free block on nonexistent device");
	if (!is_reusable(th->t_super, block, 1))
		return;
	_reiserfs_free_block(th, inode, block, 1);
}

static void __discard_prealloc(struct reiserfs_transaction_handle *th,
			       struct reiserfs_inode_info *ei)
{
	unsigned long save = ei->i_prealloc_block;
	int dirty = 0;
	struct inode *inode = &ei->vfs_inode;
	BUG_ON(!th->t_trans_id);
#ifdef CONFIG_REISERFS_CHECK
	if (ei->i_prealloc_count < 0)
		reiserfs_error(th->t_super, "zam-4001",
			       "inode has negative prealloc blocks count.");
#endif
	while (ei->i_prealloc_count > 0) {
		reiserfs_free_prealloc_block(th, inode, ei->i_prealloc_block);
		ei->i_prealloc_block++;
		ei->i_prealloc_count--;
		dirty = 1;
	}
	if (dirty)
		reiserfs_update_sd(th, inode);
	ei->i_prealloc_block = save;
	list_del_init(&(ei->i_prealloc_list));
}

void reiserfs_discard_prealloc(struct reiserfs_transaction_handle *th,
			       struct inode *inode)
{
	struct reiserfs_inode_info *ei = REISERFS_I(inode);
	BUG_ON(!th->t_trans_id);
	if (ei->i_prealloc_count)
		__discard_prealloc(th, ei);
}

void reiserfs_discard_all_prealloc(struct reiserfs_transaction_handle *th)
{
	struct list_head *plist = &SB_JOURNAL(th->t_super)->j_prealloc_list;

	BUG_ON(!th->t_trans_id);

	while (!list_empty(plist)) {
		struct reiserfs_inode_info *ei;
		ei = list_entry(plist->next, struct reiserfs_inode_info,
				i_prealloc_list);
#ifdef CONFIG_REISERFS_CHECK
		if (!ei->i_prealloc_count) {
			reiserfs_error(th->t_super, "zam-4001",
				       "inode is in prealloc list but has "
				       "no preallocated blocks.");
		}
#endif
		__discard_prealloc(th, ei);
	}
}

void reiserfs_init_alloc_options(struct super_block *s)
{
	set_bit(_ALLOC_skip_busy, &SB_ALLOC_OPTS(s));
	set_bit(_ALLOC_dirid_groups, &SB_ALLOC_OPTS(s));
	set_bit(_ALLOC_packing_groups, &SB_ALLOC_OPTS(s));
}

int reiserfs_parse_alloc_options(struct super_block *s, char *options)
{
	char *this_char, *value;

	REISERFS_SB(s)->s_alloc_options.bits = 0;	

	while ((this_char = strsep(&options, ":")) != NULL) {
		if ((value = strchr(this_char, '=')) != NULL)
			*value++ = 0;

		if (!strcmp(this_char, "concentrating_formatted_nodes")) {
			int temp;
			SET_OPTION(concentrating_formatted_nodes);
			temp = (value
				&& *value) ? simple_strtoul(value, &value,
							    0) : 10;
			if (temp <= 0 || temp > 100) {
				REISERFS_SB(s)->s_alloc_options.border = 10;
			} else {
				REISERFS_SB(s)->s_alloc_options.border =
				    100 / temp;
			}
			continue;
		}
		if (!strcmp(this_char, "displacing_large_files")) {
			SET_OPTION(displacing_large_files);
			REISERFS_SB(s)->s_alloc_options.large_file_size =
			    (value
			     && *value) ? simple_strtoul(value, &value, 0) : 16;
			continue;
		}
		if (!strcmp(this_char, "displacing_new_packing_localities")) {
			SET_OPTION(displacing_new_packing_localities);
			continue;
		};

		if (!strcmp(this_char, "old_hashed_relocation")) {
			SET_OPTION(old_hashed_relocation);
			continue;
		}

		if (!strcmp(this_char, "new_hashed_relocation")) {
			SET_OPTION(new_hashed_relocation);
			continue;
		}

		if (!strcmp(this_char, "dirid_groups")) {
			SET_OPTION(dirid_groups);
			continue;
		}
		if (!strcmp(this_char, "oid_groups")) {
			SET_OPTION(oid_groups);
			continue;
		}
		if (!strcmp(this_char, "packing_groups")) {
			SET_OPTION(packing_groups);
			continue;
		}
		if (!strcmp(this_char, "hashed_formatted_nodes")) {
			SET_OPTION(hashed_formatted_nodes);
			continue;
		}

		if (!strcmp(this_char, "skip_busy")) {
			SET_OPTION(skip_busy);
			continue;
		}

		if (!strcmp(this_char, "hundredth_slices")) {
			SET_OPTION(hundredth_slices);
			continue;
		}

		if (!strcmp(this_char, "old_way")) {
			SET_OPTION(old_way);
			continue;
		}

		if (!strcmp(this_char, "displace_based_on_dirid")) {
			SET_OPTION(displace_based_on_dirid);
			continue;
		}

		if (!strcmp(this_char, "preallocmin")) {
			REISERFS_SB(s)->s_alloc_options.preallocmin =
			    (value
			     && *value) ? simple_strtoul(value, &value, 0) : 4;
			continue;
		}

		if (!strcmp(this_char, "preallocsize")) {
			REISERFS_SB(s)->s_alloc_options.preallocsize =
			    (value
			     && *value) ? simple_strtoul(value, &value,
							 0) :
			    PREALLOCATION_SIZE;
			continue;
		}

		reiserfs_warning(s, "zam-4001", "unknown option - %s",
				 this_char);
		return 1;
	}

	reiserfs_info(s, "allocator options = [%08x]\n", SB_ALLOC_OPTS(s));
	return 0;
}

static void print_sep(struct seq_file *seq, int *first)
{
	if (!*first)
		seq_puts(seq, ":");
	else
		*first = 0;
}

void show_alloc_options(struct seq_file *seq, struct super_block *s)
{
	int first = 1;

	if (SB_ALLOC_OPTS(s) == ((1 << _ALLOC_skip_busy) |
		(1 << _ALLOC_dirid_groups) | (1 << _ALLOC_packing_groups)))
		return;

	seq_puts(seq, ",alloc=");

	if (TEST_OPTION(concentrating_formatted_nodes, s)) {
		print_sep(seq, &first);
		if (REISERFS_SB(s)->s_alloc_options.border != 10) {
			seq_printf(seq, "concentrating_formatted_nodes=%d",
				100 / REISERFS_SB(s)->s_alloc_options.border);
		} else
			seq_puts(seq, "concentrating_formatted_nodes");
	}
	if (TEST_OPTION(displacing_large_files, s)) {
		print_sep(seq, &first);
		if (REISERFS_SB(s)->s_alloc_options.large_file_size != 16) {
			seq_printf(seq, "displacing_large_files=%lu",
			    REISERFS_SB(s)->s_alloc_options.large_file_size);
		} else
			seq_puts(seq, "displacing_large_files");
	}
	if (TEST_OPTION(displacing_new_packing_localities, s)) {
		print_sep(seq, &first);
		seq_puts(seq, "displacing_new_packing_localities");
	}
	if (TEST_OPTION(old_hashed_relocation, s)) {
		print_sep(seq, &first);
		seq_puts(seq, "old_hashed_relocation");
	}
	if (TEST_OPTION(new_hashed_relocation, s)) {
		print_sep(seq, &first);
		seq_puts(seq, "new_hashed_relocation");
	}
	if (TEST_OPTION(dirid_groups, s)) {
		print_sep(seq, &first);
		seq_puts(seq, "dirid_groups");
	}
	if (TEST_OPTION(oid_groups, s)) {
		print_sep(seq, &first);
		seq_puts(seq, "oid_groups");
	}
	if (TEST_OPTION(packing_groups, s)) {
		print_sep(seq, &first);
		seq_puts(seq, "packing_groups");
	}
	if (TEST_OPTION(hashed_formatted_nodes, s)) {
		print_sep(seq, &first);
		seq_puts(seq, "hashed_formatted_nodes");
	}
	if (TEST_OPTION(skip_busy, s)) {
		print_sep(seq, &first);
		seq_puts(seq, "skip_busy");
	}
	if (TEST_OPTION(hundredth_slices, s)) {
		print_sep(seq, &first);
		seq_puts(seq, "hundredth_slices");
	}
	if (TEST_OPTION(old_way, s)) {
		print_sep(seq, &first);
		seq_puts(seq, "old_way");
	}
	if (TEST_OPTION(displace_based_on_dirid, s)) {
		print_sep(seq, &first);
		seq_puts(seq, "displace_based_on_dirid");
	}
	if (REISERFS_SB(s)->s_alloc_options.preallocmin != 0) {
		print_sep(seq, &first);
		seq_printf(seq, "preallocmin=%d",
				REISERFS_SB(s)->s_alloc_options.preallocmin);
	}
	if (REISERFS_SB(s)->s_alloc_options.preallocsize != 17) {
		print_sep(seq, &first);
		seq_printf(seq, "preallocsize=%d",
				REISERFS_SB(s)->s_alloc_options.preallocsize);
	}
}

static inline void new_hashed_relocation(reiserfs_blocknr_hint_t * hint)
{
	char *hash_in;
	if (hint->formatted_node) {
		hash_in = (char *)&hint->key.k_dir_id;
	} else {
		if (!hint->inode) {
			
			hash_in = (char *)&hint->key.k_dir_id;
		} else
		    if (TEST_OPTION(displace_based_on_dirid, hint->th->t_super))
			hash_in = (char *)(&INODE_PKEY(hint->inode)->k_dir_id);
		else
			hash_in =
			    (char *)(&INODE_PKEY(hint->inode)->k_objectid);
	}

	hint->search_start =
	    hint->beg + keyed_hash(hash_in, 4) % (hint->end - hint->beg);
}

static void dirid_groups(reiserfs_blocknr_hint_t * hint)
{
	unsigned long hash;
	__u32 dirid = 0;
	int bm = 0;
	struct super_block *sb = hint->th->t_super;
	if (hint->inode)
		dirid = le32_to_cpu(INODE_PKEY(hint->inode)->k_dir_id);
	else if (hint->formatted_node)
		dirid = hint->key.k_dir_id;

	if (dirid) {
		bm = bmap_hash_id(sb, dirid);
		hash = bm * (sb->s_blocksize << 3);
		
		if (hint->inode)
			hash += sb->s_blocksize / 2;
		hint->search_start = hash;
	}
}

static void oid_groups(reiserfs_blocknr_hint_t * hint)
{
	if (hint->inode) {
		unsigned long hash;
		__u32 oid;
		__u32 dirid;
		int bm;

		dirid = le32_to_cpu(INODE_PKEY(hint->inode)->k_dir_id);

		if (dirid <= 2)
			hash = (hint->inode->i_sb->s_blocksize << 3);
		else {
			oid = le32_to_cpu(INODE_PKEY(hint->inode)->k_objectid);
			bm = bmap_hash_id(hint->inode->i_sb, oid);
			hash = bm * (hint->inode->i_sb->s_blocksize << 3);
		}
		hint->search_start = hash;
	}
}

static int get_left_neighbor(reiserfs_blocknr_hint_t * hint)
{
	struct treepath *path;
	struct buffer_head *bh;
	struct item_head *ih;
	int pos_in_item;
	__le32 *item;
	int ret = 0;

	if (!hint->path)	
		return 0;

	path = hint->path;
	bh = get_last_bh(path);
	RFALSE(!bh, "green-4002: Illegal path specified to get_left_neighbor");
	ih = get_ih(path);
	pos_in_item = path->pos_in_item;
	item = get_item(path);

	hint->search_start = bh->b_blocknr;

	if (!hint->formatted_node && is_indirect_le_ih(ih)) {
		if (pos_in_item == I_UNFM_NUM(ih))
			pos_in_item--;
		while (pos_in_item >= 0) {
			int t = get_block_num(item, pos_in_item);
			if (t) {
				hint->search_start = t;
				ret = 1;
				break;
			}
			pos_in_item--;
		}
	}

	
	return ret;
}

static inline void set_border_in_hint(struct super_block *s,
				      reiserfs_blocknr_hint_t * hint)
{
	b_blocknr_t border =
	    SB_BLOCK_COUNT(s) / REISERFS_SB(s)->s_alloc_options.border;

	if (hint->formatted_node)
		hint->end = border - 1;
	else
		hint->beg = border;
}

static inline void displace_large_file(reiserfs_blocknr_hint_t * hint)
{
	if (TEST_OPTION(displace_based_on_dirid, hint->th->t_super))
		hint->search_start =
		    hint->beg +
		    keyed_hash((char *)(&INODE_PKEY(hint->inode)->k_dir_id),
			       4) % (hint->end - hint->beg);
	else
		hint->search_start =
		    hint->beg +
		    keyed_hash((char *)(&INODE_PKEY(hint->inode)->k_objectid),
			       4) % (hint->end - hint->beg);
}

static inline void hash_formatted_node(reiserfs_blocknr_hint_t * hint)
{
	char *hash_in;

	if (!hint->inode)
		hash_in = (char *)&hint->key.k_dir_id;
	else if (TEST_OPTION(displace_based_on_dirid, hint->th->t_super))
		hash_in = (char *)(&INODE_PKEY(hint->inode)->k_dir_id);
	else
		hash_in = (char *)(&INODE_PKEY(hint->inode)->k_objectid);

	hint->search_start =
	    hint->beg + keyed_hash(hash_in, 4) % (hint->end - hint->beg);
}

static inline int
this_blocknr_allocation_would_make_it_a_large_file(reiserfs_blocknr_hint_t *
						   hint)
{
	return hint->block ==
	    REISERFS_SB(hint->th->t_super)->s_alloc_options.large_file_size;
}

#ifdef DISPLACE_NEW_PACKING_LOCALITIES
static inline void displace_new_packing_locality(reiserfs_blocknr_hint_t * hint)
{
	struct in_core_key *key = &hint->key;

	hint->th->displace_new_blocks = 0;
	hint->search_start =
	    hint->beg + keyed_hash((char *)(&key->k_objectid),
				   4) % (hint->end - hint->beg);
}
#endif

static inline int old_hashed_relocation(reiserfs_blocknr_hint_t * hint)
{
	b_blocknr_t border;
	u32 hash_in;

	if (hint->formatted_node || hint->inode == NULL) {
		return 0;
	}

	hash_in = le32_to_cpu((INODE_PKEY(hint->inode))->k_dir_id);
	border =
	    hint->beg + (u32) keyed_hash(((char *)(&hash_in)),
					 4) % (hint->end - hint->beg - 1);
	if (border > hint->search_start)
		hint->search_start = border;

	return 1;
}

static inline int old_way(reiserfs_blocknr_hint_t * hint)
{
	b_blocknr_t border;

	if (hint->formatted_node || hint->inode == NULL) {
		return 0;
	}

	border =
	    hint->beg +
	    le32_to_cpu(INODE_PKEY(hint->inode)->k_dir_id) % (hint->end -
							      hint->beg);
	if (border > hint->search_start)
		hint->search_start = border;

	return 1;
}

static inline void hundredth_slices(reiserfs_blocknr_hint_t * hint)
{
	struct in_core_key *key = &hint->key;
	b_blocknr_t slice_start;

	slice_start =
	    (keyed_hash((char *)(&key->k_dir_id), 4) % 100) * (hint->end / 100);
	if (slice_start > hint->search_start
	    || slice_start + (hint->end / 100) <= hint->search_start) {
		hint->search_start = slice_start;
	}
}

static void determine_search_start(reiserfs_blocknr_hint_t * hint,
				   int amount_needed)
{
	struct super_block *s = hint->th->t_super;
	int unfm_hint;

	hint->beg = 0;
	hint->end = SB_BLOCK_COUNT(s) - 1;

	
	if (concentrating_formatted_nodes(s))
		set_border_in_hint(s, hint);

#ifdef DISPLACE_NEW_PACKING_LOCALITIES
	if (displacing_new_packing_localities(s)
	    && hint->th->displace_new_blocks) {
		displace_new_packing_locality(hint);

		return;
	}
#endif


	if (displacing_large_files(s) && !hint->formatted_node
	    && this_blocknr_allocation_would_make_it_a_large_file(hint)) {
		displace_large_file(hint);
		return;
	}

	if (hint->formatted_node && TEST_OPTION(hashed_formatted_nodes, s)) {
		hash_formatted_node(hint);
		return;
	}

	unfm_hint = get_left_neighbor(hint);

	if (TEST_OPTION(old_way, s)) {
		if (!hint->formatted_node) {
			if (!reiserfs_hashed_relocation(s))
				old_way(hint);
			else if (!reiserfs_no_unhashed_relocation(s))
				old_hashed_relocation(hint);

			if (hint->inode
			    && hint->search_start <
			    REISERFS_I(hint->inode)->i_prealloc_block)
				hint->search_start =
				    REISERFS_I(hint->inode)->i_prealloc_block;
		}
		return;
	}

	
	if (TEST_OPTION(hundredth_slices, s)
	    && !(displacing_large_files(s) && !hint->formatted_node)) {
		hundredth_slices(hint);
		return;
	}

	
	if (!unfm_hint && !hint->formatted_node &&
	    TEST_OPTION(old_hashed_relocation, s)) {
		old_hashed_relocation(hint);
	}
	
	if ((!unfm_hint || hint->formatted_node) &&
	    TEST_OPTION(new_hashed_relocation, s)) {
		new_hashed_relocation(hint);
	}
	
	if (!unfm_hint && !hint->formatted_node && TEST_OPTION(dirid_groups, s)) {
		dirid_groups(hint);
	}
#ifdef DISPLACE_NEW_PACKING_LOCALITIES
	if (hint->formatted_node && TEST_OPTION(dirid_groups, s)) {
		dirid_groups(hint);
	}
#endif

	
	if (!unfm_hint && !hint->formatted_node && TEST_OPTION(oid_groups, s)) {
		oid_groups(hint);
	}
	return;
}

static int determine_prealloc_size(reiserfs_blocknr_hint_t * hint)
{
	
	
	

	hint->prealloc_size = 0;

	if (!hint->formatted_node && hint->preallocate) {
		if (S_ISREG(hint->inode->i_mode)
		    && hint->inode->i_size >=
		    REISERFS_SB(hint->th->t_super)->s_alloc_options.
		    preallocmin * hint->inode->i_sb->s_blocksize)
			hint->prealloc_size =
			    REISERFS_SB(hint->th->t_super)->s_alloc_options.
			    preallocsize - 1;
	}
	return CARRY_ON;
}

static inline int allocate_without_wrapping_disk(reiserfs_blocknr_hint_t * hint,
						 b_blocknr_t * new_blocknrs,
						 b_blocknr_t start,
						 b_blocknr_t finish, int min,
						 int amount_needed,
						 int prealloc_size)
{
	int rest = amount_needed;
	int nr_allocated;

	while (rest > 0 && start <= finish) {
		nr_allocated = scan_bitmap(hint->th, &start, finish, min,
					   rest + prealloc_size,
					   !hint->formatted_node, hint->block);

		if (nr_allocated == 0)	
			break;

		
		while (rest > 0 && nr_allocated > 0) {
			*new_blocknrs++ = start++;
			rest--;
			nr_allocated--;
		}

		
		if (nr_allocated > 0) {
			
			list_add(&REISERFS_I(hint->inode)->i_prealloc_list,
				 &SB_JOURNAL(hint->th->t_super)->
				 j_prealloc_list);
			REISERFS_I(hint->inode)->i_prealloc_block = start;
			REISERFS_I(hint->inode)->i_prealloc_count =
			    nr_allocated;
			break;
		}
	}

	return (amount_needed - rest);
}

static inline int blocknrs_and_prealloc_arrays_from_search_start
    (reiserfs_blocknr_hint_t * hint, b_blocknr_t * new_blocknrs,
     int amount_needed) {
	struct super_block *s = hint->th->t_super;
	b_blocknr_t start = hint->search_start;
	b_blocknr_t finish = SB_BLOCK_COUNT(s) - 1;
	int passno = 0;
	int nr_allocated = 0;

	determine_prealloc_size(hint);
	if (!hint->formatted_node) {
		int quota_ret;
#ifdef REISERQUOTA_DEBUG
		reiserfs_debug(s, REISERFS_DEBUG_CODE,
			       "reiserquota: allocating %d blocks id=%u",
			       amount_needed, hint->inode->i_uid);
#endif
		quota_ret =
		    dquot_alloc_block_nodirty(hint->inode, amount_needed);
		if (quota_ret)	
			return QUOTA_EXCEEDED;
		if (hint->preallocate && hint->prealloc_size) {
#ifdef REISERQUOTA_DEBUG
			reiserfs_debug(s, REISERFS_DEBUG_CODE,
				       "reiserquota: allocating (prealloc) %d blocks id=%u",
				       hint->prealloc_size, hint->inode->i_uid);
#endif
			quota_ret = dquot_prealloc_block_nodirty(hint->inode,
							 hint->prealloc_size);
			if (quota_ret)
				hint->preallocate = hint->prealloc_size = 0;
		}
		
	}

	do {
		switch (passno++) {
		case 0:	
			start = hint->search_start;
			finish = SB_BLOCK_COUNT(s) - 1;
			break;
		case 1:	
			start = hint->beg;
			finish = hint->search_start;
			break;
		case 2:	
			start = 0;
			finish = hint->beg;
			break;
		default:	
			
			if (!hint->formatted_node) {
#ifdef REISERQUOTA_DEBUG
				reiserfs_debug(s, REISERFS_DEBUG_CODE,
					       "reiserquota: freeing (nospace) %d blocks id=%u",
					       amount_needed +
					       hint->prealloc_size -
					       nr_allocated,
					       hint->inode->i_uid);
#endif
				
				dquot_free_block_nodirty(hint->inode,
					amount_needed + hint->prealloc_size -
					nr_allocated);
			}
			while (nr_allocated--)
				reiserfs_free_block(hint->th, hint->inode,
						    new_blocknrs[nr_allocated],
						    !hint->formatted_node);

			return NO_DISK_SPACE;
		}
	} while ((nr_allocated += allocate_without_wrapping_disk(hint,
								 new_blocknrs +
								 nr_allocated,
								 start, finish,
								 1,
								 amount_needed -
								 nr_allocated,
								 hint->
								 prealloc_size))
		 < amount_needed);
	if (!hint->formatted_node &&
	    amount_needed + hint->prealloc_size >
	    nr_allocated + REISERFS_I(hint->inode)->i_prealloc_count) {
		
#ifdef REISERQUOTA_DEBUG
		reiserfs_debug(s, REISERFS_DEBUG_CODE,
			       "reiserquota: freeing (failed prealloc) %d blocks id=%u",
			       amount_needed + hint->prealloc_size -
			       nr_allocated -
			       REISERFS_I(hint->inode)->i_prealloc_count,
			       hint->inode->i_uid);
#endif
		dquot_free_block_nodirty(hint->inode, amount_needed +
					 hint->prealloc_size - nr_allocated -
					 REISERFS_I(hint->inode)->
					 i_prealloc_count);
	}

	return CARRY_ON;
}

static int use_preallocated_list_if_available(reiserfs_blocknr_hint_t * hint,
					      b_blocknr_t * new_blocknrs,
					      int amount_needed)
{
	struct inode *inode = hint->inode;

	if (REISERFS_I(inode)->i_prealloc_count > 0) {
		while (amount_needed) {

			*new_blocknrs++ = REISERFS_I(inode)->i_prealloc_block++;
			REISERFS_I(inode)->i_prealloc_count--;

			amount_needed--;

			if (REISERFS_I(inode)->i_prealloc_count <= 0) {
				list_del(&REISERFS_I(inode)->i_prealloc_list);
				break;
			}
		}
	}
	
	return amount_needed;
}

int reiserfs_allocate_blocknrs(reiserfs_blocknr_hint_t * hint, b_blocknr_t * new_blocknrs, int amount_needed, int reserved_by_us	
 )
{
	int initial_amount_needed = amount_needed;
	int ret;
	struct super_block *s = hint->th->t_super;

	
	if (SB_FREE_BLOCKS(s) - REISERFS_SB(s)->reserved_blocks <
	    amount_needed - reserved_by_us)
		return NO_DISK_SPACE;
	
	

	if (!hint->formatted_node && hint->preallocate) {
		amount_needed = use_preallocated_list_if_available
		    (hint, new_blocknrs, amount_needed);
		if (amount_needed == 0)	
			return CARRY_ON;
		new_blocknrs += (initial_amount_needed - amount_needed);
	}

	
	determine_search_start(hint, amount_needed);
	if (hint->search_start >= SB_BLOCK_COUNT(s))
		hint->search_start = SB_BLOCK_COUNT(s) - 1;

	
	ret = blocknrs_and_prealloc_arrays_from_search_start
	    (hint, new_blocknrs, amount_needed);


	if (ret != CARRY_ON) {
		while (amount_needed++ < initial_amount_needed) {
			reiserfs_free_block(hint->th, hint->inode,
					    *(--new_blocknrs), 1);
		}
	}
	return ret;
}

void reiserfs_cache_bitmap_metadata(struct super_block *sb,
                                    struct buffer_head *bh,
                                    struct reiserfs_bitmap_info *info)
{
	unsigned long *cur = (unsigned long *)(bh->b_data + bh->b_size);

	
	if (!reiserfs_test_le_bit(0, (unsigned long *)bh->b_data))
		reiserfs_error(sb, "reiserfs-2025", "bitmap block %lu is "
			       "corrupted: first bit must be 1", bh->b_blocknr);

	info->free_count = 0;

	while (--cur >= (unsigned long *)bh->b_data) {
		
		if (*cur == 0)
			info->free_count += BITS_PER_LONG;
		else if (*cur != ~0L)	
			info->free_count += BITS_PER_LONG - hweight_long(*cur);
	}
}

struct buffer_head *reiserfs_read_bitmap_block(struct super_block *sb,
                                               unsigned int bitmap)
{
	b_blocknr_t block = (sb->s_blocksize << 3) * bitmap;
	struct reiserfs_bitmap_info *info = SB_AP_BITMAP(sb) + bitmap;
	struct buffer_head *bh;

	if (unlikely(test_bit(REISERFS_OLD_FORMAT,
	                      &(REISERFS_SB(sb)->s_properties))))
		block = REISERFS_SB(sb)->s_sbh->b_blocknr + 1 + bitmap;
	else if (bitmap == 0)
		block = (REISERFS_DISK_OFFSET_IN_BYTES >> sb->s_blocksize_bits) + 1;

	reiserfs_write_unlock(sb);
	bh = sb_bread(sb, block);
	reiserfs_write_lock(sb);
	if (bh == NULL)
		reiserfs_warning(sb, "sh-2029: %s: bitmap block (#%u) "
		                 "reading failed", __func__, block);
	else {
		if (buffer_locked(bh)) {
			PROC_INFO_INC(sb, scan_bitmap.wait);
			reiserfs_write_unlock(sb);
			__wait_on_buffer(bh);
			reiserfs_write_lock(sb);
		}
		BUG_ON(!buffer_uptodate(bh));
		BUG_ON(atomic_read(&bh->b_count) == 0);

		if (info->free_count == UINT_MAX)
			reiserfs_cache_bitmap_metadata(sb, bh, info);
	}

	return bh;
}

int reiserfs_init_bitmap_cache(struct super_block *sb)
{
	struct reiserfs_bitmap_info *bitmap;
	unsigned int bmap_nr = reiserfs_bmap_count(sb);

	bitmap = vmalloc(sizeof(*bitmap) * bmap_nr);
	if (bitmap == NULL)
		return -ENOMEM;

	memset(bitmap, 0xff, sizeof(*bitmap) * bmap_nr);

	SB_AP_BITMAP(sb) = bitmap;

	return 0;
}

void reiserfs_free_bitmap_cache(struct super_block *sb)
{
	if (SB_AP_BITMAP(sb)) {
		vfree(SB_AP_BITMAP(sb));
		SB_AP_BITMAP(sb) = NULL;
	}
}
