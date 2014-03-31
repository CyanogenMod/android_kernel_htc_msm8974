/*
 * Copyright 2000 by Hans Reiser, licensing governed by reiserfs/README
 */

#include <asm/uaccess.h>
#include <linux/string.h>
#include <linux/time.h>
#include "reiserfs.h"
#include <linux/buffer_head.h>

int balance_internal(struct tree_balance *,
		     int, int, struct item_head *, struct buffer_head **);

#define INTERNAL_SHIFT_FROM_S_TO_L 0
#define INTERNAL_SHIFT_FROM_R_TO_S 1
#define INTERNAL_SHIFT_FROM_L_TO_S 2
#define INTERNAL_SHIFT_FROM_S_TO_R 3
#define INTERNAL_INSERT_TO_S 4
#define INTERNAL_INSERT_TO_L 5
#define INTERNAL_INSERT_TO_R 6

static void internal_define_dest_src_infos(int shift_mode,
					   struct tree_balance *tb,
					   int h,
					   struct buffer_info *dest_bi,
					   struct buffer_info *src_bi,
					   int *d_key, struct buffer_head **cf)
{
	memset(dest_bi, 0, sizeof(struct buffer_info));
	memset(src_bi, 0, sizeof(struct buffer_info));
	
	switch (shift_mode) {
	case INTERNAL_SHIFT_FROM_S_TO_L:	
		src_bi->tb = tb;
		src_bi->bi_bh = PATH_H_PBUFFER(tb->tb_path, h);
		src_bi->bi_parent = PATH_H_PPARENT(tb->tb_path, h);
		src_bi->bi_position = PATH_H_POSITION(tb->tb_path, h + 1);
		dest_bi->tb = tb;
		dest_bi->bi_bh = tb->L[h];
		dest_bi->bi_parent = tb->FL[h];
		dest_bi->bi_position = get_left_neighbor_position(tb, h);
		*d_key = tb->lkey[h];
		*cf = tb->CFL[h];
		break;
	case INTERNAL_SHIFT_FROM_L_TO_S:
		src_bi->tb = tb;
		src_bi->bi_bh = tb->L[h];
		src_bi->bi_parent = tb->FL[h];
		src_bi->bi_position = get_left_neighbor_position(tb, h);
		dest_bi->tb = tb;
		dest_bi->bi_bh = PATH_H_PBUFFER(tb->tb_path, h);
		dest_bi->bi_parent = PATH_H_PPARENT(tb->tb_path, h);
		dest_bi->bi_position = PATH_H_POSITION(tb->tb_path, h + 1);	
		*d_key = tb->lkey[h];
		*cf = tb->CFL[h];
		break;

	case INTERNAL_SHIFT_FROM_R_TO_S:	
		src_bi->tb = tb;
		src_bi->bi_bh = tb->R[h];
		src_bi->bi_parent = tb->FR[h];
		src_bi->bi_position = get_right_neighbor_position(tb, h);
		dest_bi->tb = tb;
		dest_bi->bi_bh = PATH_H_PBUFFER(tb->tb_path, h);
		dest_bi->bi_parent = PATH_H_PPARENT(tb->tb_path, h);
		dest_bi->bi_position = PATH_H_POSITION(tb->tb_path, h + 1);
		*d_key = tb->rkey[h];
		*cf = tb->CFR[h];
		break;

	case INTERNAL_SHIFT_FROM_S_TO_R:
		src_bi->tb = tb;
		src_bi->bi_bh = PATH_H_PBUFFER(tb->tb_path, h);
		src_bi->bi_parent = PATH_H_PPARENT(tb->tb_path, h);
		src_bi->bi_position = PATH_H_POSITION(tb->tb_path, h + 1);
		dest_bi->tb = tb;
		dest_bi->bi_bh = tb->R[h];
		dest_bi->bi_parent = tb->FR[h];
		dest_bi->bi_position = get_right_neighbor_position(tb, h);
		*d_key = tb->rkey[h];
		*cf = tb->CFR[h];
		break;

	case INTERNAL_INSERT_TO_L:
		dest_bi->tb = tb;
		dest_bi->bi_bh = tb->L[h];
		dest_bi->bi_parent = tb->FL[h];
		dest_bi->bi_position = get_left_neighbor_position(tb, h);
		break;

	case INTERNAL_INSERT_TO_S:
		dest_bi->tb = tb;
		dest_bi->bi_bh = PATH_H_PBUFFER(tb->tb_path, h);
		dest_bi->bi_parent = PATH_H_PPARENT(tb->tb_path, h);
		dest_bi->bi_position = PATH_H_POSITION(tb->tb_path, h + 1);
		break;

	case INTERNAL_INSERT_TO_R:
		dest_bi->tb = tb;
		dest_bi->bi_bh = tb->R[h];
		dest_bi->bi_parent = tb->FR[h];
		dest_bi->bi_position = get_right_neighbor_position(tb, h);
		break;

	default:
		reiserfs_panic(tb->tb_sb, "ibalance-1",
			       "shift type is unknown (%d)",
			       shift_mode);
	}
}

static void internal_insert_childs(struct buffer_info *cur_bi,
				   int to, int count,
				   struct item_head *inserted,
				   struct buffer_head **bh)
{
	struct buffer_head *cur = cur_bi->bi_bh;
	struct block_head *blkh;
	int nr;
	struct reiserfs_key *ih;
	struct disk_child new_dc[2];
	struct disk_child *dc;
	int i;

	if (count <= 0)
		return;

	blkh = B_BLK_HEAD(cur);
	nr = blkh_nr_item(blkh);

	RFALSE(count > 2, "too many children (%d) are to be inserted", count);
	RFALSE(B_FREE_SPACE(cur) < count * (KEY_SIZE + DC_SIZE),
	       "no enough free space (%d), needed %d bytes",
	       B_FREE_SPACE(cur), count * (KEY_SIZE + DC_SIZE));

	
	dc = B_N_CHILD(cur, to + 1);

	memmove(dc + count, dc, (nr + 1 - (to + 1)) * DC_SIZE);

	
	for (i = 0; i < count; i++) {
		put_dc_size(&(new_dc[i]),
			    MAX_CHILD_SIZE(bh[i]) - B_FREE_SPACE(bh[i]));
		put_dc_block_number(&(new_dc[i]), bh[i]->b_blocknr);
	}
	memcpy(dc, new_dc, DC_SIZE * count);

	
	ih = B_N_PDELIM_KEY(cur, ((to == -1) ? 0 : to));

	memmove(ih + count, ih,
		(nr - to) * KEY_SIZE + (nr + 1 + count) * DC_SIZE);

	
	memcpy(ih, inserted, KEY_SIZE);
	if (count > 1)
		memcpy(ih + 1, inserted + 1, KEY_SIZE);

	
	set_blkh_nr_item(blkh, blkh_nr_item(blkh) + count);
	set_blkh_free_space(blkh,
			    blkh_free_space(blkh) - count * (DC_SIZE +
							     KEY_SIZE));

	do_balance_mark_internal_dirty(cur_bi->tb, cur, 0);

	
	check_internal(cur);
	

	if (cur_bi->bi_parent) {
		struct disk_child *t_dc =
		    B_N_CHILD(cur_bi->bi_parent, cur_bi->bi_position);
		put_dc_size(t_dc,
			    dc_size(t_dc) + (count * (DC_SIZE + KEY_SIZE)));
		do_balance_mark_internal_dirty(cur_bi->tb, cur_bi->bi_parent,
					       0);

		
		check_internal(cur_bi->bi_parent);
		
	}

}

static void internal_delete_pointers_items(struct buffer_info *cur_bi,
					   int first_p,
					   int first_i, int del_num)
{
	struct buffer_head *cur = cur_bi->bi_bh;
	int nr;
	struct block_head *blkh;
	struct reiserfs_key *key;
	struct disk_child *dc;

	RFALSE(cur == NULL, "buffer is 0");
	RFALSE(del_num < 0,
	       "negative number of items (%d) can not be deleted", del_num);
	RFALSE(first_p < 0 || first_p + del_num > B_NR_ITEMS(cur) + 1
	       || first_i < 0,
	       "first pointer order (%d) < 0 or "
	       "no so many pointers (%d), only (%d) or "
	       "first key order %d < 0", first_p, first_p + del_num,
	       B_NR_ITEMS(cur) + 1, first_i);
	if (del_num == 0)
		return;

	blkh = B_BLK_HEAD(cur);
	nr = blkh_nr_item(blkh);

	if (first_p == 0 && del_num == nr + 1) {
		RFALSE(first_i != 0,
		       "1st deleted key must have order 0, not %d", first_i);
		make_empty_node(cur_bi);
		return;
	}

	RFALSE(first_i + del_num > B_NR_ITEMS(cur),
	       "first_i = %d del_num = %d "
	       "no so many keys (%d) in the node (%b)(%z)",
	       first_i, del_num, first_i + del_num, cur, cur);

	
	dc = B_N_CHILD(cur, first_p);

	memmove(dc, dc + del_num, (nr + 1 - first_p - del_num) * DC_SIZE);
	key = B_N_PDELIM_KEY(cur, first_i);
	memmove(key, key + del_num,
		(nr - first_i - del_num) * KEY_SIZE + (nr + 1 -
						       del_num) * DC_SIZE);

	
	set_blkh_nr_item(blkh, blkh_nr_item(blkh) - del_num);
	set_blkh_free_space(blkh,
			    blkh_free_space(blkh) +
			    (del_num * (KEY_SIZE + DC_SIZE)));

	do_balance_mark_internal_dirty(cur_bi->tb, cur, 0);
	
	check_internal(cur);
	

	if (cur_bi->bi_parent) {
		struct disk_child *t_dc;
		t_dc = B_N_CHILD(cur_bi->bi_parent, cur_bi->bi_position);
		put_dc_size(t_dc,
			    dc_size(t_dc) - (del_num * (KEY_SIZE + DC_SIZE)));

		do_balance_mark_internal_dirty(cur_bi->tb, cur_bi->bi_parent,
					       0);
		
		check_internal(cur_bi->bi_parent);
		
	}
}

static void internal_delete_childs(struct buffer_info *cur_bi, int from, int n)
{
	int i_from;

	i_from = (from == 0) ? from : from - 1;

	internal_delete_pointers_items(cur_bi, from, i_from, n);
}

static void internal_copy_pointers_items(struct buffer_info *dest_bi,
					 struct buffer_head *src,
					 int last_first, int cpy_num)
{
	struct buffer_head *dest = dest_bi->bi_bh;
	int nr_dest, nr_src;
	int dest_order, src_order;
	struct block_head *blkh;
	struct reiserfs_key *key;
	struct disk_child *dc;

	nr_src = B_NR_ITEMS(src);

	RFALSE(dest == NULL || src == NULL,
	       "src (%p) or dest (%p) buffer is 0", src, dest);
	RFALSE(last_first != FIRST_TO_LAST && last_first != LAST_TO_FIRST,
	       "invalid last_first parameter (%d)", last_first);
	RFALSE(nr_src < cpy_num - 1,
	       "no so many items (%d) in src (%d)", cpy_num, nr_src);
	RFALSE(cpy_num < 0, "cpy_num less than 0 (%d)", cpy_num);
	RFALSE(cpy_num - 1 + B_NR_ITEMS(dest) > (int)MAX_NR_KEY(dest),
	       "cpy_num (%d) + item number in dest (%d) can not be > MAX_NR_KEY(%d)",
	       cpy_num, B_NR_ITEMS(dest), MAX_NR_KEY(dest));

	if (cpy_num == 0)
		return;

	
	blkh = B_BLK_HEAD(dest);
	nr_dest = blkh_nr_item(blkh);

	
	
	(last_first == LAST_TO_FIRST) ? (dest_order = 0, src_order =
					 nr_src - cpy_num + 1) : (dest_order =
								  nr_dest,
								  src_order =
								  0);

	
	dc = B_N_CHILD(dest, dest_order);

	memmove(dc + cpy_num, dc, (nr_dest - dest_order) * DC_SIZE);

	
	memcpy(dc, B_N_CHILD(src, src_order), DC_SIZE * cpy_num);

	
	key = B_N_PDELIM_KEY(dest, dest_order);
	memmove(key + cpy_num - 1, key,
		KEY_SIZE * (nr_dest - dest_order) + DC_SIZE * (nr_dest +
							       cpy_num));

	
	memcpy(key, B_N_PDELIM_KEY(src, src_order), KEY_SIZE * (cpy_num - 1));

	
	set_blkh_nr_item(blkh, blkh_nr_item(blkh) + (cpy_num - 1));
	set_blkh_free_space(blkh,
			    blkh_free_space(blkh) - (KEY_SIZE * (cpy_num - 1) +
						     DC_SIZE * cpy_num));

	do_balance_mark_internal_dirty(dest_bi->tb, dest, 0);

	
	check_internal(dest);
	

	if (dest_bi->bi_parent) {
		struct disk_child *t_dc;
		t_dc = B_N_CHILD(dest_bi->bi_parent, dest_bi->bi_position);
		put_dc_size(t_dc,
			    dc_size(t_dc) + (KEY_SIZE * (cpy_num - 1) +
					     DC_SIZE * cpy_num));

		do_balance_mark_internal_dirty(dest_bi->tb, dest_bi->bi_parent,
					       0);
		
		check_internal(dest_bi->bi_parent);
		
	}

}

static void internal_move_pointers_items(struct buffer_info *dest_bi,
					 struct buffer_info *src_bi,
					 int last_first, int cpy_num,
					 int del_par)
{
	int first_pointer;
	int first_item;

	internal_copy_pointers_items(dest_bi, src_bi->bi_bh, last_first,
				     cpy_num);

	if (last_first == FIRST_TO_LAST) {	
		first_pointer = 0;
		first_item = 0;
		internal_delete_pointers_items(src_bi, first_pointer,
					       first_item, cpy_num - del_par);
	} else {		
		int i, j;

		i = (cpy_num - del_par ==
		     (j =
		      B_NR_ITEMS(src_bi->bi_bh)) + 1) ? 0 : j - cpy_num +
		    del_par;

		internal_delete_pointers_items(src_bi,
					       j + 1 - cpy_num + del_par, i,
					       cpy_num - del_par);
	}
}

static void internal_insert_key(struct buffer_info *dest_bi, int dest_position_before,	
				struct buffer_head *src, int src_position)
{
	struct buffer_head *dest = dest_bi->bi_bh;
	int nr;
	struct block_head *blkh;
	struct reiserfs_key *key;

	RFALSE(dest == NULL || src == NULL,
	       "source(%p) or dest(%p) buffer is 0", src, dest);
	RFALSE(dest_position_before < 0 || src_position < 0,
	       "source(%d) or dest(%d) key number less than 0",
	       src_position, dest_position_before);
	RFALSE(dest_position_before > B_NR_ITEMS(dest) ||
	       src_position >= B_NR_ITEMS(src),
	       "invalid position in dest (%d (key number %d)) or in src (%d (key number %d))",
	       dest_position_before, B_NR_ITEMS(dest),
	       src_position, B_NR_ITEMS(src));
	RFALSE(B_FREE_SPACE(dest) < KEY_SIZE,
	       "no enough free space (%d) in dest buffer", B_FREE_SPACE(dest));

	blkh = B_BLK_HEAD(dest);
	nr = blkh_nr_item(blkh);

	
	key = B_N_PDELIM_KEY(dest, dest_position_before);
	memmove(key + 1, key,
		(nr - dest_position_before) * KEY_SIZE + (nr + 1) * DC_SIZE);

	
	memcpy(key, B_N_PDELIM_KEY(src, src_position), KEY_SIZE);

	

	set_blkh_nr_item(blkh, blkh_nr_item(blkh) + 1);
	set_blkh_free_space(blkh, blkh_free_space(blkh) - KEY_SIZE);

	do_balance_mark_internal_dirty(dest_bi->tb, dest, 0);

	if (dest_bi->bi_parent) {
		struct disk_child *t_dc;
		t_dc = B_N_CHILD(dest_bi->bi_parent, dest_bi->bi_position);
		put_dc_size(t_dc, dc_size(t_dc) + KEY_SIZE);

		do_balance_mark_internal_dirty(dest_bi->tb, dest_bi->bi_parent,
					       0);
	}
}

static void internal_shift_left(int mode,	
				struct tree_balance *tb,
				int h, int pointer_amount)
{
	struct buffer_info dest_bi, src_bi;
	struct buffer_head *cf;
	int d_key_position;

	internal_define_dest_src_infos(mode, tb, h, &dest_bi, &src_bi,
				       &d_key_position, &cf);

	

	if (pointer_amount) {
		
		internal_insert_key(&dest_bi, B_NR_ITEMS(dest_bi.bi_bh), cf,
				    d_key_position);

		if (B_NR_ITEMS(src_bi.bi_bh) == pointer_amount - 1) {
			if (src_bi.bi_position   == 0)
				replace_key(tb, cf, d_key_position,
					    src_bi.
					    bi_parent  , 0);
		} else
			replace_key(tb, cf, d_key_position, src_bi.bi_bh,
				    pointer_amount - 1);
	}
	
	internal_move_pointers_items(&dest_bi, &src_bi, FIRST_TO_LAST,
				     pointer_amount, 0);

}

static void internal_shift1_left(struct tree_balance *tb,
				 int h, int pointer_amount)
{
	struct buffer_info dest_bi, src_bi;
	struct buffer_head *cf;
	int d_key_position;

	internal_define_dest_src_infos(INTERNAL_SHIFT_FROM_S_TO_L, tb, h,
				       &dest_bi, &src_bi, &d_key_position, &cf);

	if (pointer_amount > 0)	
		internal_insert_key(&dest_bi, B_NR_ITEMS(dest_bi.bi_bh), cf,
				    d_key_position);
	

	
	internal_move_pointers_items(&dest_bi, &src_bi, FIRST_TO_LAST,
				     pointer_amount, 1);
	
}

static void internal_shift_right(int mode,	
				 struct tree_balance *tb,
				 int h, int pointer_amount)
{
	struct buffer_info dest_bi, src_bi;
	struct buffer_head *cf;
	int d_key_position;
	int nr;

	internal_define_dest_src_infos(mode, tb, h, &dest_bi, &src_bi,
				       &d_key_position, &cf);

	nr = B_NR_ITEMS(src_bi.bi_bh);

	if (pointer_amount > 0) {
		
		internal_insert_key(&dest_bi, 0, cf, d_key_position);
		if (nr == pointer_amount - 1) {
			RFALSE(src_bi.bi_bh != PATH_H_PBUFFER(tb->tb_path, h)  ||
			       dest_bi.bi_bh != tb->R[h],
			       "src (%p) must be == tb->S[h](%p) when it disappears",
			       src_bi.bi_bh, PATH_H_PBUFFER(tb->tb_path, h));
			
			if (tb->CFL[h])
				replace_key(tb, cf, d_key_position, tb->CFL[h],
					    tb->lkey[h]);
		} else
			replace_key(tb, cf, d_key_position, src_bi.bi_bh,
				    nr - pointer_amount);
	}

	
	internal_move_pointers_items(&dest_bi, &src_bi, LAST_TO_FIRST,
				     pointer_amount, 0);
}

static void internal_shift1_right(struct tree_balance *tb,
				  int h, int pointer_amount)
{
	struct buffer_info dest_bi, src_bi;
	struct buffer_head *cf;
	int d_key_position;

	internal_define_dest_src_infos(INTERNAL_SHIFT_FROM_S_TO_R, tb, h,
				       &dest_bi, &src_bi, &d_key_position, &cf);

	if (pointer_amount > 0)	
		internal_insert_key(&dest_bi, 0, cf, d_key_position);
	

	
	internal_move_pointers_items(&dest_bi, &src_bi, LAST_TO_FIRST,
				     pointer_amount, 1);
	
}

static void balance_internal_when_delete(struct tree_balance *tb,
					 int h, int child_pos)
{
	int insert_num;
	int n;
	struct buffer_head *tbSh = PATH_H_PBUFFER(tb->tb_path, h);
	struct buffer_info bi;

	insert_num = tb->insert_size[h] / ((int)(DC_SIZE + KEY_SIZE));

	
	bi.tb = tb;
	bi.bi_bh = tbSh;
	bi.bi_parent = PATH_H_PPARENT(tb->tb_path, h);
	bi.bi_position = PATH_H_POSITION(tb->tb_path, h + 1);

	internal_delete_childs(&bi, child_pos, -insert_num);

	RFALSE(tb->blknum[h] > 1,
	       "tb->blknum[%d]=%d when insert_size < 0", h, tb->blknum[h]);

	n = B_NR_ITEMS(tbSh);

	if (tb->lnum[h] == 0 && tb->rnum[h] == 0) {
		if (tb->blknum[h] == 0) {
			
			struct buffer_head *new_root;

			RFALSE(n
			       || B_FREE_SPACE(tbSh) !=
			       MAX_CHILD_SIZE(tbSh) - DC_SIZE,
			       "buffer must have only 0 keys (%d)", n);
			RFALSE(bi.bi_parent, "root has parent (%p)",
			       bi.bi_parent);

			
			if (!tb->L[h - 1] || !B_NR_ITEMS(tb->L[h - 1]))
				new_root = tb->R[h - 1];
			else
				new_root = tb->L[h - 1];
			
			PUT_SB_ROOT_BLOCK(tb->tb_sb, new_root->b_blocknr);
			
			PUT_SB_TREE_HEIGHT(tb->tb_sb,
					   SB_TREE_HEIGHT(tb->tb_sb) - 1);

			do_balance_mark_sb_dirty(tb,
						 REISERFS_SB(tb->tb_sb)->s_sbh,
						 1);
			
			if (h > 1)
				
				check_internal(new_root);
			

			
			reiserfs_invalidate_buffer(tb, tbSh);
			return;
		}
		return;
	}

	if (tb->L[h] && tb->lnum[h] == -B_NR_ITEMS(tb->L[h]) - 1) {	

		RFALSE(tb->rnum[h] != 0,
		       "invalid tb->rnum[%d]==%d when joining S[h] with L[h]",
		       h, tb->rnum[h]);

		internal_shift_left(INTERNAL_SHIFT_FROM_S_TO_L, tb, h, n + 1);
		reiserfs_invalidate_buffer(tb, tbSh);

		return;
	}

	if (tb->R[h] && tb->rnum[h] == -B_NR_ITEMS(tb->R[h]) - 1) {	
		RFALSE(tb->lnum[h] != 0,
		       "invalid tb->lnum[%d]==%d when joining S[h] with R[h]",
		       h, tb->lnum[h]);

		internal_shift_right(INTERNAL_SHIFT_FROM_S_TO_R, tb, h, n + 1);

		reiserfs_invalidate_buffer(tb, tbSh);
		return;
	}

	if (tb->lnum[h] < 0) {	
		RFALSE(tb->rnum[h] != 0,
		       "wrong tb->rnum[%d]==%d when borrow from L[h]", h,
		       tb->rnum[h]);
		
		internal_shift_right(INTERNAL_SHIFT_FROM_L_TO_S, tb, h,
				     -tb->lnum[h]);
		return;
	}

	if (tb->rnum[h] < 0) {	
		RFALSE(tb->lnum[h] != 0,
		       "invalid tb->lnum[%d]==%d when borrow from R[h]",
		       h, tb->lnum[h]);
		internal_shift_left(INTERNAL_SHIFT_FROM_R_TO_S, tb, h, -tb->rnum[h]);	
		return;
	}

	if (tb->lnum[h] > 0) {	
		RFALSE(tb->rnum[h] == 0 || tb->lnum[h] + tb->rnum[h] != n + 1,
		       "invalid tb->lnum[%d]==%d or tb->rnum[%d]==%d when S[h](item number == %d) is split between them",
		       h, tb->lnum[h], h, tb->rnum[h], n);

		internal_shift_left(INTERNAL_SHIFT_FROM_S_TO_L, tb, h, tb->lnum[h]);	
		internal_shift_right(INTERNAL_SHIFT_FROM_S_TO_R, tb, h,
				     tb->rnum[h]);

		reiserfs_invalidate_buffer(tb, tbSh);

		return;
	}
	reiserfs_panic(tb->tb_sb, "ibalance-2",
		       "unexpected tb->lnum[%d]==%d or tb->rnum[%d]==%d",
		       h, tb->lnum[h], h, tb->rnum[h]);
}

static void replace_lkey(struct tree_balance *tb, int h, struct item_head *key)
{
	RFALSE(tb->L[h] == NULL || tb->CFL[h] == NULL,
	       "L[h](%p) and CFL[h](%p) must exist in replace_lkey",
	       tb->L[h], tb->CFL[h]);

	if (B_NR_ITEMS(PATH_H_PBUFFER(tb->tb_path, h)) == 0)
		return;

	memcpy(B_N_PDELIM_KEY(tb->CFL[h], tb->lkey[h]), key, KEY_SIZE);

	do_balance_mark_internal_dirty(tb, tb->CFL[h], 0);
}

static void replace_rkey(struct tree_balance *tb, int h, struct item_head *key)
{
	RFALSE(tb->R[h] == NULL || tb->CFR[h] == NULL,
	       "R[h](%p) and CFR[h](%p) must exist in replace_rkey",
	       tb->R[h], tb->CFR[h]);
	RFALSE(B_NR_ITEMS(tb->R[h]) == 0,
	       "R[h] can not be empty if it exists (item number=%d)",
	       B_NR_ITEMS(tb->R[h]));

	memcpy(B_N_PDELIM_KEY(tb->CFR[h], tb->rkey[h]), key, KEY_SIZE);

	do_balance_mark_internal_dirty(tb, tb->CFR[h], 0);
}

int balance_internal(struct tree_balance *tb,	
		     int h,	
		     int child_pos, struct item_head *insert_key,	
		     struct buffer_head **insert_ptr	
    )
{
	struct buffer_head *tbSh = PATH_H_PBUFFER(tb->tb_path, h);
	struct buffer_info bi;
	int order;		
	int insert_num, n, k;
	struct buffer_head *S_new;
	struct item_head new_insert_key;
	struct buffer_head *new_insert_ptr = NULL;
	struct item_head *new_insert_key_addr = insert_key;

	RFALSE(h < 1, "h (%d) can not be < 1 on internal level", h);

	PROC_INFO_INC(tb->tb_sb, balance_at[h]);

	order =
	    (tbSh) ? PATH_H_POSITION(tb->tb_path,
				     h + 1)  : 0;

	insert_num = tb->insert_size[h] / ((int)(KEY_SIZE + DC_SIZE));

	
	RFALSE(insert_num < -2 || insert_num > 2,
	       "incorrect number of items inserted to the internal node (%d)",
	       insert_num);
	RFALSE(h > 1 && (insert_num > 1 || insert_num < -1),
	       "incorrect number of items (%d) inserted to the internal node on a level (h=%d) higher than last internal level",
	       insert_num, h);

	
	if (insert_num < 0) {
		balance_internal_when_delete(tb, h, child_pos);
		return order;
	}

	k = 0;
	if (tb->lnum[h] > 0) {
		n = B_NR_ITEMS(tb->L[h]);	
		if (tb->lnum[h] <= child_pos) {
			
			internal_shift_left(INTERNAL_SHIFT_FROM_S_TO_L, tb, h,
					    tb->lnum[h]);
			
			child_pos -= tb->lnum[h];
		} else if (tb->lnum[h] > child_pos + insert_num) {
			
			internal_shift_left(INTERNAL_SHIFT_FROM_S_TO_L, tb, h,
					    tb->lnum[h] - insert_num);
			
			bi.tb = tb;
			bi.bi_bh = tb->L[h];
			bi.bi_parent = tb->FL[h];
			bi.bi_position = get_left_neighbor_position(tb, h);
			internal_insert_childs(&bi,
					       
					       n + child_pos + 1,
					       insert_num, insert_key,
					       insert_ptr);

			insert_num = 0;
		} else {
			struct disk_child *dc;

			
			internal_shift1_left(tb, h, child_pos + 1);
			
			k = tb->lnum[h] - child_pos - 1;
			bi.tb = tb;
			bi.bi_bh = tb->L[h];
			bi.bi_parent = tb->FL[h];
			bi.bi_position = get_left_neighbor_position(tb, h);
			internal_insert_childs(&bi,
					       
					       n + child_pos + 1, k,
					       insert_key, insert_ptr);

			replace_lkey(tb, h, insert_key + k);

			
			dc = B_N_CHILD(tbSh, 0);
			put_dc_size(dc,
				    MAX_CHILD_SIZE(insert_ptr[k]) -
				    B_FREE_SPACE(insert_ptr[k]));
			put_dc_block_number(dc, insert_ptr[k]->b_blocknr);

			do_balance_mark_internal_dirty(tb, tbSh, 0);

			k++;
			insert_key += k;
			insert_ptr += k;
			insert_num -= k;
			child_pos = 0;
		}
	}
	
	if (tb->rnum[h] > 0) {
		
		
		n = B_NR_ITEMS(tbSh);	
		if (n - tb->rnum[h] >= child_pos)
			
			
			internal_shift_right(INTERNAL_SHIFT_FROM_S_TO_R, tb, h,
					     tb->rnum[h]);
		else if (n + insert_num - tb->rnum[h] < child_pos) {
			
			internal_shift_right(INTERNAL_SHIFT_FROM_S_TO_R, tb, h,
					     tb->rnum[h] - insert_num);

			
			bi.tb = tb;
			bi.bi_bh = tb->R[h];
			bi.bi_parent = tb->FR[h];
			bi.bi_position = get_right_neighbor_position(tb, h);
			internal_insert_childs(&bi,
					       
					       child_pos - n - insert_num +
					       tb->rnum[h] - 1,
					       insert_num, insert_key,
					       insert_ptr);
			insert_num = 0;
		} else {
			struct disk_child *dc;

			
			internal_shift1_right(tb, h, n - child_pos + 1);
			
			k = tb->rnum[h] - n + child_pos - 1;
			bi.tb = tb;
			bi.bi_bh = tb->R[h];
			bi.bi_parent = tb->FR[h];
			bi.bi_position = get_right_neighbor_position(tb, h);
			internal_insert_childs(&bi,
					       
					       0, k, insert_key + 1,
					       insert_ptr + 1);

			replace_rkey(tb, h, insert_key + insert_num - k - 1);

			
			dc = B_N_CHILD(tb->R[h], 0);
			put_dc_size(dc,
				    MAX_CHILD_SIZE(insert_ptr
						   [insert_num - k - 1]) -
				    B_FREE_SPACE(insert_ptr
						 [insert_num - k - 1]));
			put_dc_block_number(dc,
					    insert_ptr[insert_num - k -
						       1]->b_blocknr);

			do_balance_mark_internal_dirty(tb, tb->R[h], 0);

			insert_num -= (k + 1);
		}
	}

    
	RFALSE(tb->blknum[h] > 2, "blknum can not be > 2 for internal level");
	RFALSE(tb->blknum[h] < 0, "blknum can not be < 0");

	if (!tb->blknum[h]) {	
		RFALSE(!tbSh, "S[h] is equal NULL");

		
		reiserfs_invalidate_buffer(tb, tbSh);
		return order;
	}

	if (!tbSh) {
		
		struct disk_child *dc;
		struct buffer_head *tbSh_1 = PATH_H_PBUFFER(tb->tb_path, h - 1);
		struct block_head *blkh;

		if (tb->blknum[h] != 1)
			reiserfs_panic(NULL, "ibalance-3", "One new node "
				       "required for creating the new root");
		
		tbSh = get_FEB(tb);
		blkh = B_BLK_HEAD(tbSh);
		set_blkh_level(blkh, h + 1);

		

		dc = B_N_CHILD(tbSh, 0);
		put_dc_block_number(dc, tbSh_1->b_blocknr);
		put_dc_size(dc,
			    (MAX_CHILD_SIZE(tbSh_1) - B_FREE_SPACE(tbSh_1)));

		tb->insert_size[h] -= DC_SIZE;
		set_blkh_free_space(blkh, blkh_free_space(blkh) - DC_SIZE);

		do_balance_mark_internal_dirty(tb, tbSh, 0);

		
		check_internal(tbSh);
		

		
		PATH_OFFSET_PBUFFER(tb->tb_path, ILLEGAL_PATH_ELEMENT_OFFSET) =
		    tbSh;

		
		PUT_SB_ROOT_BLOCK(tb->tb_sb, tbSh->b_blocknr);
		PUT_SB_TREE_HEIGHT(tb->tb_sb, SB_TREE_HEIGHT(tb->tb_sb) + 1);
		do_balance_mark_sb_dirty(tb, REISERFS_SB(tb->tb_sb)->s_sbh, 1);
	}

	if (tb->blknum[h] == 2) {
		int snum;
		struct buffer_info dest_bi, src_bi;

		
		S_new = get_FEB(tb);

		set_blkh_level(B_BLK_HEAD(S_new), h + 1);

		dest_bi.tb = tb;
		dest_bi.bi_bh = S_new;
		dest_bi.bi_parent = NULL;
		dest_bi.bi_position = 0;
		src_bi.tb = tb;
		src_bi.bi_bh = tbSh;
		src_bi.bi_parent = PATH_H_PPARENT(tb->tb_path, h);
		src_bi.bi_position = PATH_H_POSITION(tb->tb_path, h + 1);

		n = B_NR_ITEMS(tbSh);	
		snum = (insert_num + n + 1) / 2;
		if (n - snum >= child_pos) {
			
			
			
			memcpy(&new_insert_key, B_N_PDELIM_KEY(tbSh, n - snum),
			       KEY_SIZE);
			
			internal_move_pointers_items(&dest_bi, &src_bi,
						     LAST_TO_FIRST, snum, 0);
			
		} else if (n + insert_num - snum < child_pos) {
			
			
			
			memcpy(&new_insert_key,
			       B_N_PDELIM_KEY(tbSh, n + insert_num - snum),
			       KEY_SIZE);
			
			internal_move_pointers_items(&dest_bi, &src_bi,
						     LAST_TO_FIRST,
						     snum - insert_num, 0);
			

			
			internal_insert_childs(&dest_bi,
					       
					       child_pos - n - insert_num +
					       snum - 1,
					       insert_num, insert_key,
					       insert_ptr);

			insert_num = 0;
		} else {
			struct disk_child *dc;

			
			
			internal_move_pointers_items(&dest_bi, &src_bi,
						     LAST_TO_FIRST,
						     n - child_pos + 1, 1);
			
			
			k = snum - n + child_pos - 1;

			internal_insert_childs(&dest_bi,  0, k,
					       insert_key + 1, insert_ptr + 1);

			
			memcpy(&new_insert_key, insert_key + insert_num - k - 1,
			       KEY_SIZE);
			

			dc = B_N_CHILD(S_new, 0);
			put_dc_size(dc,
				    (MAX_CHILD_SIZE
				     (insert_ptr[insert_num - k - 1]) -
				     B_FREE_SPACE(insert_ptr
						  [insert_num - k - 1])));
			put_dc_block_number(dc,
					    insert_ptr[insert_num - k -
						       1]->b_blocknr);

			do_balance_mark_internal_dirty(tb, S_new, 0);

			insert_num -= (k + 1);
		}
		
		new_insert_ptr = S_new;

		RFALSE(!buffer_journaled(S_new) || buffer_journal_dirty(S_new)
		       || buffer_dirty(S_new), "cm-00001: bad S_new (%b)",
		       S_new);

		
	}

	n = B_NR_ITEMS(tbSh);	

	if (0 <= child_pos && child_pos <= n && insert_num > 0) {
		bi.tb = tb;
		bi.bi_bh = tbSh;
		bi.bi_parent = PATH_H_PPARENT(tb->tb_path, h);
		bi.bi_position = PATH_H_POSITION(tb->tb_path, h + 1);
		internal_insert_childs(&bi,	
				       
				       child_pos, insert_num, insert_key,
				       insert_ptr);
	}

	memcpy(new_insert_key_addr, &new_insert_key, KEY_SIZE);
	insert_ptr[0] = new_insert_ptr;

	return order;
}
