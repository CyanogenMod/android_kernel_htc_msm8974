/*
 * Copyright 2000 by Hans Reiser, licensing governed by reiserfs/README
 */


#include <linux/time.h>
#include <linux/slab.h>
#include <linux/string.h>
#include "reiserfs.h"
#include <linux/buffer_head.h>


static inline int old_item_num(int new_num, int affected_item_num, int mode)
{
	if (mode == M_PASTE || mode == M_CUT || new_num < affected_item_num)
		return new_num;

	if (mode == M_INSERT) {

		RFALSE(new_num == 0,
		       "vs-8005: for INSERT mode and item number of inserted item");

		return new_num - 1;
	}

	RFALSE(mode != M_DELETE,
	       "vs-8010: old_item_num: mode must be M_DELETE (mode = \'%c\'",
	       mode);
	
	return new_num + 1;
}

static void create_virtual_node(struct tree_balance *tb, int h)
{
	struct item_head *ih;
	struct virtual_node *vn = tb->tb_vn;
	int new_num;
	struct buffer_head *Sh;	

	Sh = PATH_H_PBUFFER(tb->tb_path, h);

	
	vn->vn_size =
	    MAX_CHILD_SIZE(Sh) - B_FREE_SPACE(Sh) + tb->insert_size[h];

	
	if (h) {
		vn->vn_nr_item = (vn->vn_size - DC_SIZE) / (DC_SIZE + KEY_SIZE);
		return;
	}

	
	vn->vn_nr_item =
	    B_NR_ITEMS(Sh) + ((vn->vn_mode == M_INSERT) ? 1 : 0) -
	    ((vn->vn_mode == M_DELETE) ? 1 : 0);

	
	vn->vn_vi = (struct virtual_item *)(tb->tb_vn + 1);
	memset(vn->vn_vi, 0, vn->vn_nr_item * sizeof(struct virtual_item));
	vn->vn_free_ptr += vn->vn_nr_item * sizeof(struct virtual_item);

	
	ih = B_N_PITEM_HEAD(Sh, 0);

	
	if (op_is_left_mergeable(&(ih->ih_key), Sh->b_size)
	    && (vn->vn_mode != M_DELETE || vn->vn_affected_item_num))
		vn->vn_vi[0].vi_type |= VI_TYPE_LEFT_MERGEABLE;

	
	for (new_num = 0; new_num < vn->vn_nr_item; new_num++) {
		int j;
		struct virtual_item *vi = vn->vn_vi + new_num;
		int is_affected =
		    ((new_num != vn->vn_affected_item_num) ? 0 : 1);

		if (is_affected && vn->vn_mode == M_INSERT)
			continue;

		
		j = old_item_num(new_num, vn->vn_affected_item_num,
				 vn->vn_mode);

		vi->vi_item_len += ih_item_len(ih + j) + IH_SIZE;
		vi->vi_ih = ih + j;
		vi->vi_item = B_I_PITEM(Sh, ih + j);
		vi->vi_uarea = vn->vn_free_ptr;

		
		
		vn->vn_free_ptr +=
		    op_create_vi(vn, vi, is_affected, tb->insert_size[0]);
		if (tb->vn_buf + tb->vn_buf_size < vn->vn_free_ptr)
			reiserfs_panic(tb->tb_sb, "vs-8030",
				       "virtual node space consumed");

		if (!is_affected)
			
			continue;

		if (vn->vn_mode == M_PASTE || vn->vn_mode == M_CUT) {
			vn->vn_vi[new_num].vi_item_len += tb->insert_size[0];
			vi->vi_new_data = vn->vn_data;	
		}
	}

	
	if (vn->vn_mode == M_INSERT) {
		struct virtual_item *vi = vn->vn_vi + vn->vn_affected_item_num;

		RFALSE(vn->vn_ins_ih == NULL,
		       "vs-8040: item header of inserted item is not specified");
		vi->vi_item_len = tb->insert_size[0];
		vi->vi_ih = vn->vn_ins_ih;
		vi->vi_item = vn->vn_data;
		vi->vi_uarea = vn->vn_free_ptr;

		op_create_vi(vn, vi, 0  ,
			     tb->insert_size[0]);
	}

	
	if (tb->CFR[0]) {
		struct reiserfs_key *key;

		key = B_N_PDELIM_KEY(tb->CFR[0], tb->rkey[0]);
		if (op_is_left_mergeable(key, Sh->b_size)
		    && (vn->vn_mode != M_DELETE
			|| vn->vn_affected_item_num != B_NR_ITEMS(Sh) - 1))
			vn->vn_vi[vn->vn_nr_item - 1].vi_type |=
			    VI_TYPE_RIGHT_MERGEABLE;

#ifdef CONFIG_REISERFS_CHECK
		if (op_is_left_mergeable(key, Sh->b_size) &&
		    !(vn->vn_mode != M_DELETE
		      || vn->vn_affected_item_num != B_NR_ITEMS(Sh) - 1)) {
			
			if (!
			    (B_NR_ITEMS(Sh) == 1
			     && is_direntry_le_ih(B_N_PITEM_HEAD(Sh, 0))
			     && I_ENTRY_COUNT(B_N_PITEM_HEAD(Sh, 0)) == 1)) {
				
				print_block(Sh, 0, -1, -1);
				reiserfs_panic(tb->tb_sb, "vs-8045",
					       "rdkey %k, affected item==%d "
					       "(mode==%c) Must be %c",
					       key, vn->vn_affected_item_num,
					       vn->vn_mode, M_DELETE);
			}
		}
#endif

	}
}

static void check_left(struct tree_balance *tb, int h, int cur_free)
{
	int i;
	struct virtual_node *vn = tb->tb_vn;
	struct virtual_item *vi;
	int d_size, ih_size;

	RFALSE(cur_free < 0, "vs-8050: cur_free (%d) < 0", cur_free);

	
	if (h > 0) {
		tb->lnum[h] = cur_free / (DC_SIZE + KEY_SIZE);
		return;
	}

	

	if (!cur_free || !vn->vn_nr_item) {
		
		tb->lnum[h] = 0;
		tb->lbytes = -1;
		return;
	}

	RFALSE(!PATH_H_PPARENT(tb->tb_path, 0),
	       "vs-8055: parent does not exist or invalid");

	vi = vn->vn_vi;
	if ((unsigned int)cur_free >=
	    (vn->vn_size -
	     ((vi->vi_type & VI_TYPE_LEFT_MERGEABLE) ? IH_SIZE : 0))) {
		

		RFALSE(vn->vn_mode == M_INSERT || vn->vn_mode == M_PASTE,
		       "vs-8055: invalid mode or balance condition failed");

		tb->lnum[0] = vn->vn_nr_item;
		tb->lbytes = -1;
		return;
	}

	d_size = 0, ih_size = IH_SIZE;

	
	if (vi->vi_type & VI_TYPE_LEFT_MERGEABLE)
		d_size = -((int)IH_SIZE), ih_size = 0;

	tb->lnum[0] = 0;
	for (i = 0; i < vn->vn_nr_item;
	     i++, ih_size = IH_SIZE, d_size = 0, vi++) {
		d_size += vi->vi_item_len;
		if (cur_free >= d_size) {
			
			cur_free -= d_size;
			tb->lnum[0]++;
			continue;
		}

		
		
		if (cur_free <= ih_size) {
			
			tb->lbytes = -1;
			return;
		}
		cur_free -= ih_size;

		tb->lbytes = op_check_left(vi, cur_free, 0, 0);
		if (tb->lbytes != -1)
			
			tb->lnum[0]++;

		break;
	}

	return;
}

static void check_right(struct tree_balance *tb, int h, int cur_free)
{
	int i;
	struct virtual_node *vn = tb->tb_vn;
	struct virtual_item *vi;
	int d_size, ih_size;

	RFALSE(cur_free < 0, "vs-8070: cur_free < 0");

	
	if (h > 0) {
		tb->rnum[h] = cur_free / (DC_SIZE + KEY_SIZE);
		return;
	}

	

	if (!cur_free || !vn->vn_nr_item) {
		
		tb->rnum[h] = 0;
		tb->rbytes = -1;
		return;
	}

	RFALSE(!PATH_H_PPARENT(tb->tb_path, 0),
	       "vs-8075: parent does not exist or invalid");

	vi = vn->vn_vi + vn->vn_nr_item - 1;
	if ((unsigned int)cur_free >=
	    (vn->vn_size -
	     ((vi->vi_type & VI_TYPE_RIGHT_MERGEABLE) ? IH_SIZE : 0))) {
		

		RFALSE(vn->vn_mode == M_INSERT || vn->vn_mode == M_PASTE,
		       "vs-8080: invalid mode or balance condition failed");

		tb->rnum[h] = vn->vn_nr_item;
		tb->rbytes = -1;
		return;
	}

	d_size = 0, ih_size = IH_SIZE;

	
	if (vi->vi_type & VI_TYPE_RIGHT_MERGEABLE)
		d_size = -(int)IH_SIZE, ih_size = 0;

	tb->rnum[0] = 0;
	for (i = vn->vn_nr_item - 1; i >= 0;
	     i--, d_size = 0, ih_size = IH_SIZE, vi--) {
		d_size += vi->vi_item_len;
		if (cur_free >= d_size) {
			
			cur_free -= d_size;
			tb->rnum[0]++;
			continue;
		}

		
		if (cur_free <= ih_size) {	
			tb->rbytes = -1;
			return;
		}

		
		cur_free -= ih_size;	

		tb->rbytes = op_check_right(vi, cur_free);
		if (tb->rbytes != -1)
			
			tb->rnum[0]++;

		break;
	}

	return;
}

static int get_num_ver(int mode, struct tree_balance *tb, int h,
		       int from, int from_bytes,
		       int to, int to_bytes, short *snum012, int flow)
{
	int i;
	int cur_free;
	
	int units;
	struct virtual_node *vn = tb->tb_vn;
	

	int total_node_size, max_node_size, current_item_size;
	int needed_nodes;
	int start_item,		
	 end_item,		
	 start_bytes,		
	 end_bytes;		
	int split_item_positions[2];	

	split_item_positions[0] = -1;
	split_item_positions[1] = -1;

	RFALSE(tb->insert_size[h] < 0 || (mode != M_INSERT && mode != M_PASTE),
	       "vs-8100: insert_size < 0 in overflow");

	max_node_size = MAX_CHILD_SIZE(PATH_H_PBUFFER(tb->tb_path, h));

	snum012[3] = -1;	
	snum012[4] = -1;	

	
	if (h > 0) {
		i = ((to - from) * (KEY_SIZE + DC_SIZE) + DC_SIZE);
		if (i == max_node_size)
			return 1;
		return (i / max_node_size + 1);
	}

	
	needed_nodes = 1;
	total_node_size = 0;
	cur_free = max_node_size;

	
	start_item = from;
	
	start_bytes = ((from_bytes != -1) ? from_bytes : 0);

	
	end_item = vn->vn_nr_item - to - 1;
	
	end_bytes = (to_bytes != -1) ? to_bytes : 0;


	for (i = start_item; i <= end_item; i++) {
		struct virtual_item *vi = vn->vn_vi + i;
		int skip_from_end = ((i == end_item) ? end_bytes : 0);

		RFALSE(needed_nodes > 3, "vs-8105: too many nodes are needed");

		
		current_item_size = vi->vi_item_len;

		
		current_item_size -=
		    op_part_size(vi, 0  , start_bytes);

		
		current_item_size -=
		    op_part_size(vi, 1  , skip_from_end);

		
		if (total_node_size + current_item_size <= max_node_size) {
			snum012[needed_nodes - 1]++;
			total_node_size += current_item_size;
			start_bytes = 0;
			continue;
		}

		if (current_item_size > max_node_size) {
			RFALSE(is_direct_le_ih(vi->vi_ih),
			       "vs-8110: "
			       "direct item length is %d. It can not be longer than %d",
			       current_item_size, max_node_size);
			
			flow = 1;
		}

		if (!flow) {
			
			needed_nodes++;
			i--;
			total_node_size = 0;
			continue;
		}
		
		
		{
			int free_space;

			free_space = max_node_size - total_node_size - IH_SIZE;
			units =
			    op_check_left(vi, free_space, start_bytes,
					  skip_from_end);
			if (units == -1) {
				
				needed_nodes++, i--, total_node_size = 0;
				continue;
			}
		}

		
		
		
		
		start_bytes += units;
		snum012[needed_nodes - 1 + 3] = units;

		if (needed_nodes > 2)
			reiserfs_warning(tb->tb_sb, "vs-8111",
					 "split_item_position is out of range");
		snum012[needed_nodes - 1]++;
		split_item_positions[needed_nodes - 1] = i;
		needed_nodes++;
		
		start_item = i;
		i--;
		total_node_size = 0;
	}

	
	
	
	if (snum012[4] > 0) {
		int split_item_num;
		int bytes_to_r, bytes_to_l;
		int bytes_to_S1new;

		split_item_num = split_item_positions[1];
		bytes_to_l =
		    ((from == split_item_num
		      && from_bytes != -1) ? from_bytes : 0);
		bytes_to_r =
		    ((end_item == split_item_num
		      && end_bytes != -1) ? end_bytes : 0);
		bytes_to_S1new =
		    ((split_item_positions[0] ==
		      split_item_positions[1]) ? snum012[3] : 0);

		
		snum012[4] =
		    op_unit_num(&vn->vn_vi[split_item_num]) - snum012[4] -
		    bytes_to_r - bytes_to_l - bytes_to_S1new;

		if (vn->vn_vi[split_item_num].vi_index != TYPE_DIRENTRY &&
		    vn->vn_vi[split_item_num].vi_index != TYPE_INDIRECT)
			reiserfs_warning(tb->tb_sb, "vs-8115",
					 "not directory or indirect item");
	}

	
	if (snum012[3] > 0) {
		int split_item_num;
		int bytes_to_r, bytes_to_l;
		int bytes_to_S2new;

		split_item_num = split_item_positions[0];
		bytes_to_l =
		    ((from == split_item_num
		      && from_bytes != -1) ? from_bytes : 0);
		bytes_to_r =
		    ((end_item == split_item_num
		      && end_bytes != -1) ? end_bytes : 0);
		bytes_to_S2new =
		    ((split_item_positions[0] == split_item_positions[1]
		      && snum012[4] != -1) ? snum012[4] : 0);

		
		snum012[3] =
		    op_unit_num(&vn->vn_vi[split_item_num]) - snum012[3] -
		    bytes_to_r - bytes_to_l - bytes_to_S2new;
	}

	return needed_nodes;
}



static void set_parameters(struct tree_balance *tb, int h, int lnum,
			   int rnum, int blk_num, short *s012, int lb, int rb)
{

	tb->lnum[h] = lnum;
	tb->rnum[h] = rnum;
	tb->blknum[h] = blk_num;

	if (h == 0) {		
		if (s012 != NULL) {
			tb->s0num = *s012++,
			    tb->s1num = *s012++, tb->s2num = *s012++;
			tb->s1bytes = *s012++;
			tb->s2bytes = *s012;
		}
		tb->lbytes = lb;
		tb->rbytes = rb;
	}
	PROC_INFO_ADD(tb->tb_sb, lnum[h], lnum);
	PROC_INFO_ADD(tb->tb_sb, rnum[h], rnum);

	PROC_INFO_ADD(tb->tb_sb, lbytes[h], lb);
	PROC_INFO_ADD(tb->tb_sb, rbytes[h], rb);
}

static int is_leaf_removable(struct tree_balance *tb)
{
	struct virtual_node *vn = tb->tb_vn;
	int to_left, to_right;
	int size;
	int remain_items;

	to_left = tb->lnum[0] - ((tb->lbytes != -1) ? 1 : 0);
	to_right = tb->rnum[0] - ((tb->rbytes != -1) ? 1 : 0);
	remain_items = vn->vn_nr_item;

	
	remain_items -= (to_left + to_right);

	if (remain_items < 1) {
		
		set_parameters(tb, 0, to_left, vn->vn_nr_item - to_left, 0,
			       NULL, -1, -1);
		return 1;
	}

	if (remain_items > 1 || tb->lbytes == -1 || tb->rbytes == -1)
		
		return 0;

	

	
	size = op_unit_num(&(vn->vn_vi[to_left]));

	if (tb->lbytes + tb->rbytes >= size) {
		set_parameters(tb, 0, to_left + 1, to_right + 1, 0, NULL,
			       tb->lbytes, -1);
		return 1;
	}

	return 0;
}

static int are_leaves_removable(struct tree_balance *tb, int lfree, int rfree)
{
	struct virtual_node *vn = tb->tb_vn;
	int ih_size;
	struct buffer_head *S0;

	S0 = PATH_H_PBUFFER(tb->tb_path, 0);

	ih_size = 0;
	if (vn->vn_nr_item) {
		if (vn->vn_vi[0].vi_type & VI_TYPE_LEFT_MERGEABLE)
			ih_size += IH_SIZE;

		if (vn->vn_vi[vn->vn_nr_item - 1].
		    vi_type & VI_TYPE_RIGHT_MERGEABLE)
			ih_size += IH_SIZE;
	} else {
		
		struct item_head *ih;

		RFALSE(B_NR_ITEMS(S0) != 1,
		       "vs-8125: item number must be 1: it is %d",
		       B_NR_ITEMS(S0));

		ih = B_N_PITEM_HEAD(S0, 0);
		if (tb->CFR[0]
		    && !comp_short_le_keys(&(ih->ih_key),
					   B_N_PDELIM_KEY(tb->CFR[0],
							  tb->rkey[0])))
			if (is_direntry_le_ih(ih)) {
				ih_size = IH_SIZE;

				RFALSE(le_ih_k_offset(ih) == DOT_OFFSET,
				       "vs-8130: first directory item can not be removed until directory is not empty");
			}

	}

	if (MAX_CHILD_SIZE(S0) + vn->vn_size <= rfree + lfree + ih_size) {
		set_parameters(tb, 0, -1, -1, -1, NULL, -1, -1);
		PROC_INFO_INC(tb->tb_sb, leaves_removable);
		return 1;
	}
	return 0;

}

#define SET_PAR_SHIFT_LEFT \
if (h)\
{\
   int to_l;\
   \
   to_l = (MAX_NR_KEY(Sh)+1 - lpar + vn->vn_nr_item + 1) / 2 -\
	      (MAX_NR_KEY(Sh) + 1 - lpar);\
	      \
	      set_parameters (tb, h, to_l, 0, lnver, NULL, -1, -1);\
}\
else \
{\
   if (lset==LEFT_SHIFT_FLOW)\
     set_parameters (tb, h, lpar, 0, lnver, snum012+lset,\
		     tb->lbytes, -1);\
   else\
     set_parameters (tb, h, lpar - (tb->lbytes!=-1), 0, lnver, snum012+lset,\
		     -1, -1);\
}

#define SET_PAR_SHIFT_RIGHT \
if (h)\
{\
   int to_r;\
   \
   to_r = (MAX_NR_KEY(Sh)+1 - rpar + vn->vn_nr_item + 1) / 2 - (MAX_NR_KEY(Sh) + 1 - rpar);\
   \
   set_parameters (tb, h, 0, to_r, rnver, NULL, -1, -1);\
}\
else \
{\
   if (rset==RIGHT_SHIFT_FLOW)\
     set_parameters (tb, h, 0, rpar, rnver, snum012+rset,\
		  -1, tb->rbytes);\
   else\
     set_parameters (tb, h, 0, rpar - (tb->rbytes!=-1), rnver, snum012+rset,\
		  -1, -1);\
}

static void free_buffers_in_tb(struct tree_balance *tb)
{
	int i;

	pathrelse(tb->tb_path);

	for (i = 0; i < MAX_HEIGHT; i++) {
		brelse(tb->L[i]);
		brelse(tb->R[i]);
		brelse(tb->FL[i]);
		brelse(tb->FR[i]);
		brelse(tb->CFL[i]);
		brelse(tb->CFR[i]);

		tb->L[i] = NULL;
		tb->R[i] = NULL;
		tb->FL[i] = NULL;
		tb->FR[i] = NULL;
		tb->CFL[i] = NULL;
		tb->CFR[i] = NULL;
	}
}

static int get_empty_nodes(struct tree_balance *tb, int h)
{
	struct buffer_head *new_bh,
	    *Sh = PATH_H_PBUFFER(tb->tb_path, h);
	b_blocknr_t *blocknr, blocknrs[MAX_AMOUNT_NEEDED] = { 0, };
	int counter, number_of_freeblk, amount_needed,	
	 retval = CARRY_ON;
	struct super_block *sb = tb->tb_sb;


	
	for (counter = 0, number_of_freeblk = tb->cur_blknum;
	     counter < h; counter++)
		number_of_freeblk -=
		    (tb->blknum[counter]) ? (tb->blknum[counter] -
						   1) : 0;

	
	
	amount_needed = (Sh) ? (tb->blknum[h] - 1) : 1;
	
	if (amount_needed > number_of_freeblk)
		amount_needed -= number_of_freeblk;
	else			
		return CARRY_ON;

	
	if (reiserfs_new_form_blocknrs(tb, blocknrs,
				       amount_needed) == NO_DISK_SPACE)
		return NO_DISK_SPACE;

	
	for (blocknr = blocknrs, counter = 0;
	     counter < amount_needed; blocknr++, counter++) {

		RFALSE(!*blocknr,
		       "PAP-8135: reiserfs_new_blocknrs failed when got new blocks");

		new_bh = sb_getblk(sb, *blocknr);
		RFALSE(buffer_dirty(new_bh) ||
		       buffer_journaled(new_bh) ||
		       buffer_journal_dirty(new_bh),
		       "PAP-8140: journaled or dirty buffer %b for the new block",
		       new_bh);

		
		RFALSE(tb->FEB[tb->cur_blknum],
		       "PAP-8141: busy slot for new buffer");

		set_buffer_journal_new(new_bh);
		tb->FEB[tb->cur_blknum++] = new_bh;
	}

	if (retval == CARRY_ON && FILESYSTEM_CHANGED_TB(tb))
		retval = REPEAT_SEARCH;

	return retval;
}

static int get_lfree(struct tree_balance *tb, int h)
{
	struct buffer_head *l, *f;
	int order;

	if ((f = PATH_H_PPARENT(tb->tb_path, h)) == NULL ||
	    (l = tb->FL[h]) == NULL)
		return 0;

	if (f == l)
		order = PATH_H_B_ITEM_ORDER(tb->tb_path, h) - 1;
	else {
		order = B_NR_ITEMS(l);
		f = l;
	}

	return (MAX_CHILD_SIZE(f) - dc_size(B_N_CHILD(f, order)));
}

static int get_rfree(struct tree_balance *tb, int h)
{
	struct buffer_head *r, *f;
	int order;

	if ((f = PATH_H_PPARENT(tb->tb_path, h)) == NULL ||
	    (r = tb->FR[h]) == NULL)
		return 0;

	if (f == r)
		order = PATH_H_B_ITEM_ORDER(tb->tb_path, h) + 1;
	else {
		order = 0;
		f = r;
	}

	return (MAX_CHILD_SIZE(f) - dc_size(B_N_CHILD(f, order)));

}

static int is_left_neighbor_in_cache(struct tree_balance *tb, int h)
{
	struct buffer_head *father, *left;
	struct super_block *sb = tb->tb_sb;
	b_blocknr_t left_neighbor_blocknr;
	int left_neighbor_position;

	
	if (!tb->FL[h])
		return 0;

	
	father = PATH_H_PBUFFER(tb->tb_path, h + 1);

	RFALSE(!father ||
	       !B_IS_IN_TREE(father) ||
	       !B_IS_IN_TREE(tb->FL[h]) ||
	       !buffer_uptodate(father) ||
	       !buffer_uptodate(tb->FL[h]),
	       "vs-8165: F[h] (%b) or FL[h] (%b) is invalid",
	       father, tb->FL[h]);

	
	left_neighbor_position = (father == tb->FL[h]) ?
	    tb->lkey[h] : B_NR_ITEMS(tb->FL[h]);
	
	left_neighbor_blocknr =
	    B_N_CHILD_NUM(tb->FL[h], left_neighbor_position);
	
	if ((left = sb_find_get_block(sb, left_neighbor_blocknr))) {

		RFALSE(buffer_uptodate(left) && !B_IS_IN_TREE(left),
		       "vs-8170: left neighbor (%b %z) is not in the tree",
		       left, left);
		put_bh(left);
		return 1;
	}

	return 0;
}

#define LEFT_PARENTS  'l'
#define RIGHT_PARENTS 'r'

static void decrement_key(struct cpu_key *key)
{
	
	item_ops[cpu_key_k_type(key)]->decrement_key(key);
}

static int get_far_parent(struct tree_balance *tb,
			  int h,
			  struct buffer_head **pfather,
			  struct buffer_head **pcom_father, char c_lr_par)
{
	struct buffer_head *parent;
	INITIALIZE_PATH(s_path_to_neighbor_father);
	struct treepath *path = tb->tb_path;
	struct cpu_key s_lr_father_key;
	int counter,
	    position = INT_MAX,
	    first_last_position = 0,
	    path_offset = PATH_H_PATH_OFFSET(path, h);


	counter = path_offset;

	RFALSE(counter < FIRST_PATH_ELEMENT_OFFSET,
	       "PAP-8180: invalid path length");

	for (; counter > FIRST_PATH_ELEMENT_OFFSET; counter--) {
		
		if (!B_IS_IN_TREE
		    (parent = PATH_OFFSET_PBUFFER(path, counter - 1)))
			return REPEAT_SEARCH;
		
		if ((position =
		     PATH_OFFSET_POSITION(path,
					  counter - 1)) >
		    B_NR_ITEMS(parent))
			return REPEAT_SEARCH;
		
		if (B_N_CHILD_NUM(parent, position) !=
		    PATH_OFFSET_PBUFFER(path, counter)->b_blocknr)
			return REPEAT_SEARCH;
		
		if (c_lr_par == RIGHT_PARENTS)
			first_last_position = B_NR_ITEMS(parent);
		if (position != first_last_position) {
			*pcom_father = parent;
			get_bh(*pcom_father);
			
			break;
		}
	}

	
	if (counter == FIRST_PATH_ELEMENT_OFFSET) {
		
		if (PATH_OFFSET_PBUFFER
		    (tb->tb_path,
		     FIRST_PATH_ELEMENT_OFFSET)->b_blocknr ==
		    SB_ROOT_BLOCK(tb->tb_sb)) {
			*pfather = *pcom_father = NULL;
			return CARRY_ON;
		}
		return REPEAT_SEARCH;
	}

	RFALSE(B_LEVEL(*pcom_father) <= DISK_LEAF_NODE_LEVEL,
	       "PAP-8185: (%b %z) level too small",
	       *pcom_father, *pcom_father);

	

	if (buffer_locked(*pcom_father)) {

		
		reiserfs_write_unlock(tb->tb_sb);
		__wait_on_buffer(*pcom_father);
		reiserfs_write_lock(tb->tb_sb);
		if (FILESYSTEM_CHANGED_TB(tb)) {
			brelse(*pcom_father);
			return REPEAT_SEARCH;
		}
	}


	
	le_key2cpu_key(&s_lr_father_key,
		       B_N_PDELIM_KEY(*pcom_father,
				      (c_lr_par ==
				       LEFT_PARENTS) ? (tb->lkey[h - 1] =
							position -
							1) : (tb->rkey[h -
									   1] =
							      position)));

	if (c_lr_par == LEFT_PARENTS)
		decrement_key(&s_lr_father_key);

	if (search_by_key
	    (tb->tb_sb, &s_lr_father_key, &s_path_to_neighbor_father,
	     h + 1) == IO_ERROR)
		
		return IO_ERROR;

	if (FILESYSTEM_CHANGED_TB(tb)) {
		pathrelse(&s_path_to_neighbor_father);
		brelse(*pcom_father);
		return REPEAT_SEARCH;
	}

	*pfather = PATH_PLAST_BUFFER(&s_path_to_neighbor_father);

	RFALSE(B_LEVEL(*pfather) != h + 1,
	       "PAP-8190: (%b %z) level too small", *pfather, *pfather);
	RFALSE(s_path_to_neighbor_father.path_length <
	       FIRST_PATH_ELEMENT_OFFSET, "PAP-8192: path length is too small");

	s_path_to_neighbor_father.path_length--;
	pathrelse(&s_path_to_neighbor_father);
	return CARRY_ON;
}

static int get_parents(struct tree_balance *tb, int h)
{
	struct treepath *path = tb->tb_path;
	int position,
	    ret,
	    path_offset = PATH_H_PATH_OFFSET(tb->tb_path, h);
	struct buffer_head *curf, *curcf;

	
	if (path_offset <= FIRST_PATH_ELEMENT_OFFSET) {
		brelse(tb->FL[h]);
		brelse(tb->CFL[h]);
		brelse(tb->FR[h]);
		brelse(tb->CFR[h]);
		tb->FL[h]  = NULL;
		tb->CFL[h] = NULL;
		tb->FR[h]  = NULL;
		tb->CFR[h] = NULL;
		return CARRY_ON;
	}

	
	position = PATH_OFFSET_POSITION(path, path_offset - 1);
	if (position) {
		
		curf = PATH_OFFSET_PBUFFER(path, path_offset - 1);
		curcf = PATH_OFFSET_PBUFFER(path, path_offset - 1);
		get_bh(curf);
		get_bh(curf);
		tb->lkey[h] = position - 1;
	} else {
		if ((ret = get_far_parent(tb, h + 1, &curf,
						  &curcf,
						  LEFT_PARENTS)) != CARRY_ON)
			return ret;
	}

	brelse(tb->FL[h]);
	tb->FL[h] = curf;	
	brelse(tb->CFL[h]);
	tb->CFL[h] = curcf;	

	RFALSE((curf && !B_IS_IN_TREE(curf)) ||
	       (curcf && !B_IS_IN_TREE(curcf)),
	       "PAP-8195: FL (%b) or CFL (%b) is invalid", curf, curcf);


	if (position == B_NR_ITEMS(PATH_H_PBUFFER(path, h + 1))) {
		if ((ret =
		     get_far_parent(tb, h + 1, &curf, &curcf,
				    RIGHT_PARENTS)) != CARRY_ON)
			return ret;
	} else {
		curf = PATH_OFFSET_PBUFFER(path, path_offset - 1);
		curcf = PATH_OFFSET_PBUFFER(path, path_offset - 1);
		get_bh(curf);
		get_bh(curf);
		tb->rkey[h] = position;
	}

	brelse(tb->FR[h]);
	
	tb->FR[h] = curf;

	brelse(tb->CFR[h]);
	
	tb->CFR[h] = curcf;

	RFALSE((curf && !B_IS_IN_TREE(curf)) ||
	       (curcf && !B_IS_IN_TREE(curcf)),
	       "PAP-8205: FR (%b) or CFR (%b) is invalid", curf, curcf);

	return CARRY_ON;
}

static inline int can_node_be_removed(int mode, int lfree, int sfree, int rfree,
				      struct tree_balance *tb, int h)
{
	struct buffer_head *Sh = PATH_H_PBUFFER(tb->tb_path, h);
	int levbytes = tb->insert_size[h];
	struct item_head *ih;
	struct reiserfs_key *r_key = NULL;

	ih = B_N_PITEM_HEAD(Sh, 0);
	if (tb->CFR[h])
		r_key = B_N_PDELIM_KEY(tb->CFR[h], tb->rkey[h]);

	if (lfree + rfree + sfree < MAX_CHILD_SIZE(Sh) + levbytes
	    
	    -
	    ((!h
	      && op_is_left_mergeable(&(ih->ih_key), Sh->b_size)) ? IH_SIZE : 0)
	    -
	    ((!h && r_key
	      && op_is_left_mergeable(r_key, Sh->b_size)) ? IH_SIZE : 0)
	    + ((h) ? KEY_SIZE : 0)) {
		
		if (sfree >= levbytes) {	
			if (!h)
				tb->s0num =
				    B_NR_ITEMS(Sh) +
				    ((mode == M_INSERT) ? 1 : 0);
			set_parameters(tb, h, 0, 0, 1, NULL, -1, -1);
			return NO_BALANCING_NEEDED;
		}
	}
	PROC_INFO_INC(tb->tb_sb, can_node_be_removed[h]);
	return !NO_BALANCING_NEEDED;
}

static int ip_check_balance(struct tree_balance *tb, int h)
{
	struct virtual_node *vn = tb->tb_vn;
	int levbytes,		
	 ret;

	int lfree, sfree, rfree  ;

	int nver, lnver, rnver, lrnver;


	short snum012[40] = { 0, };	

	
	struct buffer_head *Sh;

	Sh = PATH_H_PBUFFER(tb->tb_path, h);
	levbytes = tb->insert_size[h];

	
	if (!Sh) {
		if (!h)
			reiserfs_panic(tb->tb_sb, "vs-8210",
				       "S[0] can not be 0");
		switch (ret = get_empty_nodes(tb, h)) {
		case CARRY_ON:
			set_parameters(tb, h, 0, 0, 1, NULL, -1, -1);
			return NO_BALANCING_NEEDED;	

		case NO_DISK_SPACE:
		case REPEAT_SEARCH:
			return ret;
		default:
			reiserfs_panic(tb->tb_sb, "vs-8215", "incorrect "
				       "return value of get_empty_nodes");
		}
	}

	if ((ret = get_parents(tb, h)) != CARRY_ON)	
		return ret;

	sfree = B_FREE_SPACE(Sh);

	
	rfree = get_rfree(tb, h);
	lfree = get_lfree(tb, h);

	if (can_node_be_removed(vn->vn_mode, lfree, sfree, rfree, tb, h) ==
	    NO_BALANCING_NEEDED)
		
		return NO_BALANCING_NEEDED;

	create_virtual_node(tb, h);

	check_left(tb, h, lfree);

	check_right(tb, h, rfree);

	if (h && (tb->rnum[h] + tb->lnum[h] >= vn->vn_nr_item + 1)) {
		int to_r;

		to_r =
		    ((MAX_NR_KEY(Sh) << 1) + 2 - tb->lnum[h] - tb->rnum[h] +
		     vn->vn_nr_item + 1) / 2 - (MAX_NR_KEY(Sh) + 1 -
						tb->rnum[h]);
		set_parameters(tb, h, vn->vn_nr_item + 1 - to_r, to_r, 0, NULL,
			       -1, -1);
		return CARRY_ON;
	}

	
	RFALSE(h &&
	       (tb->lnum[h] >= vn->vn_nr_item + 1 ||
		tb->rnum[h] >= vn->vn_nr_item + 1),
	       "vs-8220: tree is not balanced on internal level");
	RFALSE(!h && ((tb->lnum[h] >= vn->vn_nr_item && (tb->lbytes == -1)) ||
		      (tb->rnum[h] >= vn->vn_nr_item && (tb->rbytes == -1))),
	       "vs-8225: tree is not balanced on leaf level");

	if (!h && is_leaf_removable(tb))
		return CARRY_ON;

	if (sfree >= levbytes) {	
		if (!h)
			tb->s0num = vn->vn_nr_item;
		set_parameters(tb, h, 0, 0, 1, NULL, -1, -1);
		return NO_BALANCING_NEEDED;
	}

	{
		int lpar, rpar, nset, lset, rset, lrset;

#define FLOW 1
#define NO_FLOW 0		

		
#define NOTHING_SHIFT_NO_FLOW	0
#define NOTHING_SHIFT_FLOW	5
#define LEFT_SHIFT_NO_FLOW	10
#define LEFT_SHIFT_FLOW		15
#define RIGHT_SHIFT_NO_FLOW	20
#define RIGHT_SHIFT_FLOW	25
#define LR_SHIFT_NO_FLOW	30
#define LR_SHIFT_FLOW		35

		lpar = tb->lnum[h];
		rpar = tb->rnum[h];

		nset = NOTHING_SHIFT_NO_FLOW;
		nver = get_num_ver(vn->vn_mode, tb, h,
				   0, -1, h ? vn->vn_nr_item : 0, -1,
				   snum012, NO_FLOW);

		if (!h) {
			int nver1;

			
			nver1 = get_num_ver(vn->vn_mode, tb, h,
					    0, -1, 0, -1,
					    snum012 + NOTHING_SHIFT_FLOW, FLOW);
			if (nver > nver1)
				nset = NOTHING_SHIFT_FLOW, nver = nver1;
		}

		lset = LEFT_SHIFT_NO_FLOW;
		lnver = get_num_ver(vn->vn_mode, tb, h,
				    lpar - ((h || tb->lbytes == -1) ? 0 : 1),
				    -1, h ? vn->vn_nr_item : 0, -1,
				    snum012 + LEFT_SHIFT_NO_FLOW, NO_FLOW);
		if (!h) {
			int lnver1;

			lnver1 = get_num_ver(vn->vn_mode, tb, h,
					     lpar -
					     ((tb->lbytes != -1) ? 1 : 0),
					     tb->lbytes, 0, -1,
					     snum012 + LEFT_SHIFT_FLOW, FLOW);
			if (lnver > lnver1)
				lset = LEFT_SHIFT_FLOW, lnver = lnver1;
		}

		rset = RIGHT_SHIFT_NO_FLOW;
		rnver = get_num_ver(vn->vn_mode, tb, h,
				    0, -1,
				    h ? (vn->vn_nr_item - rpar) : (rpar -
								   ((tb->
								     rbytes !=
								     -1) ? 1 :
								    0)), -1,
				    snum012 + RIGHT_SHIFT_NO_FLOW, NO_FLOW);
		if (!h) {
			int rnver1;

			rnver1 = get_num_ver(vn->vn_mode, tb, h,
					     0, -1,
					     (rpar -
					      ((tb->rbytes != -1) ? 1 : 0)),
					     tb->rbytes,
					     snum012 + RIGHT_SHIFT_FLOW, FLOW);

			if (rnver > rnver1)
				rset = RIGHT_SHIFT_FLOW, rnver = rnver1;
		}

		lrset = LR_SHIFT_NO_FLOW;
		lrnver = get_num_ver(vn->vn_mode, tb, h,
				     lpar - ((h || tb->lbytes == -1) ? 0 : 1),
				     -1,
				     h ? (vn->vn_nr_item - rpar) : (rpar -
								    ((tb->
								      rbytes !=
								      -1) ? 1 :
								     0)), -1,
				     snum012 + LR_SHIFT_NO_FLOW, NO_FLOW);
		if (!h) {
			int lrnver1;

			lrnver1 = get_num_ver(vn->vn_mode, tb, h,
					      lpar -
					      ((tb->lbytes != -1) ? 1 : 0),
					      tb->lbytes,
					      (rpar -
					       ((tb->rbytes != -1) ? 1 : 0)),
					      tb->rbytes,
					      snum012 + LR_SHIFT_FLOW, FLOW);
			if (lrnver > lrnver1)
				lrset = LR_SHIFT_FLOW, lrnver = lrnver1;
		}


		
		if (lrnver < lnver && lrnver < rnver) {
			RFALSE(h &&
			       (tb->lnum[h] != 1 ||
				tb->rnum[h] != 1 ||
				lrnver != 1 || rnver != 2 || lnver != 2
				|| h != 1), "vs-8230: bad h");
			if (lrset == LR_SHIFT_FLOW)
				set_parameters(tb, h, tb->lnum[h], tb->rnum[h],
					       lrnver, snum012 + lrset,
					       tb->lbytes, tb->rbytes);
			else
				set_parameters(tb, h,
					       tb->lnum[h] -
					       ((tb->lbytes == -1) ? 0 : 1),
					       tb->rnum[h] -
					       ((tb->rbytes == -1) ? 0 : 1),
					       lrnver, snum012 + lrset, -1, -1);

			return CARRY_ON;
		}

		
		if (nver == lrnver) {
			set_parameters(tb, h, 0, 0, nver, snum012 + nset, -1,
				       -1);
			return CARRY_ON;
		}


		
		if (lnver < rnver) {
			SET_PAR_SHIFT_LEFT;
			return CARRY_ON;
		}

		
		if (lnver > rnver) {
			SET_PAR_SHIFT_RIGHT;
			return CARRY_ON;
		}

		if (is_left_neighbor_in_cache(tb, h)) {
			SET_PAR_SHIFT_LEFT;
			return CARRY_ON;
		}

		
		SET_PAR_SHIFT_RIGHT;
		return CARRY_ON;
	}
}

static int dc_check_balance_internal(struct tree_balance *tb, int h)
{
	struct virtual_node *vn = tb->tb_vn;

	struct buffer_head *Sh, *Fh;
	int maxsize, ret;
	int lfree, rfree  ;

	Sh = PATH_H_PBUFFER(tb->tb_path, h);
	Fh = PATH_H_PPARENT(tb->tb_path, h);

	maxsize = MAX_CHILD_SIZE(Sh);

	create_virtual_node(tb, h);

	if (!Fh) {		
		if (vn->vn_nr_item > 0) {
			set_parameters(tb, h, 0, 0, 1, NULL, -1, -1);
			return NO_BALANCING_NEEDED;	
		}
		set_parameters(tb, h, 0, 0, 0, NULL, -1, -1);
		return CARRY_ON;
	}

	if ((ret = get_parents(tb, h)) != CARRY_ON)
		return ret;

	
	rfree = get_rfree(tb, h);
	lfree = get_lfree(tb, h);

	
	check_left(tb, h, lfree);
	check_right(tb, h, rfree);

	if (vn->vn_nr_item >= MIN_NR_KEY(Sh)) {	
		if (vn->vn_nr_item == MIN_NR_KEY(Sh)) {	
			if (tb->lnum[h] >= vn->vn_nr_item + 1) {
				
				int n;
				int order_L;

				order_L =
				    ((n =
				      PATH_H_B_ITEM_ORDER(tb->tb_path,
							  h)) ==
				     0) ? B_NR_ITEMS(tb->FL[h]) : n - 1;
				n = dc_size(B_N_CHILD(tb->FL[h], order_L)) /
				    (DC_SIZE + KEY_SIZE);
				set_parameters(tb, h, -n - 1, 0, 0, NULL, -1,
					       -1);
				return CARRY_ON;
			}

			if (tb->rnum[h] >= vn->vn_nr_item + 1) {
				
				int n;
				int order_R;

				order_R =
				    ((n =
				      PATH_H_B_ITEM_ORDER(tb->tb_path,
							  h)) ==
				     B_NR_ITEMS(Fh)) ? 0 : n + 1;
				n = dc_size(B_N_CHILD(tb->FR[h], order_R)) /
				    (DC_SIZE + KEY_SIZE);
				set_parameters(tb, h, 0, -n - 1, 0, NULL, -1,
					       -1);
				return CARRY_ON;
			}
		}

		if (tb->rnum[h] + tb->lnum[h] >= vn->vn_nr_item + 1) {
			
			int to_r;

			to_r =
			    ((MAX_NR_KEY(Sh) << 1) + 2 - tb->lnum[h] -
			     tb->rnum[h] + vn->vn_nr_item + 1) / 2 -
			    (MAX_NR_KEY(Sh) + 1 - tb->rnum[h]);
			set_parameters(tb, h, vn->vn_nr_item + 1 - to_r, to_r,
				       0, NULL, -1, -1);
			return CARRY_ON;
		}

		
		set_parameters(tb, h, 0, 0, 1, NULL, -1, -1);
		return NO_BALANCING_NEEDED;
	}

	
	
	if (tb->lnum[h] >= vn->vn_nr_item + 1)
		if (is_left_neighbor_in_cache(tb, h)
		    || tb->rnum[h] < vn->vn_nr_item + 1 || !tb->FR[h]) {
			int n;
			int order_L;

			order_L =
			    ((n =
			      PATH_H_B_ITEM_ORDER(tb->tb_path,
						  h)) ==
			     0) ? B_NR_ITEMS(tb->FL[h]) : n - 1;
			n = dc_size(B_N_CHILD(tb->FL[h], order_L)) / (DC_SIZE +
								      KEY_SIZE);
			set_parameters(tb, h, -n - 1, 0, 0, NULL, -1, -1);
			return CARRY_ON;
		}

	
	if (tb->rnum[h] >= vn->vn_nr_item + 1) {
		int n;
		int order_R;

		order_R =
		    ((n =
		      PATH_H_B_ITEM_ORDER(tb->tb_path,
					  h)) == B_NR_ITEMS(Fh)) ? 0 : (n + 1);
		n = dc_size(B_N_CHILD(tb->FR[h], order_R)) / (DC_SIZE +
							      KEY_SIZE);
		set_parameters(tb, h, 0, -n - 1, 0, NULL, -1, -1);
		return CARRY_ON;
	}

	
	if (tb->rnum[h] + tb->lnum[h] >= vn->vn_nr_item + 1) {
		int to_r;

		to_r =
		    ((MAX_NR_KEY(Sh) << 1) + 2 - tb->lnum[h] - tb->rnum[h] +
		     vn->vn_nr_item + 1) / 2 - (MAX_NR_KEY(Sh) + 1 -
						tb->rnum[h]);
		set_parameters(tb, h, vn->vn_nr_item + 1 - to_r, to_r, 0, NULL,
			       -1, -1);
		return CARRY_ON;
	}

	
	RFALSE(!tb->FL[h] && !tb->FR[h], "vs-8235: trying to borrow for root");

	
	if (is_left_neighbor_in_cache(tb, h) || !tb->FR[h]) {
		int from_l;

		from_l =
		    (MAX_NR_KEY(Sh) + 1 - tb->lnum[h] + vn->vn_nr_item +
		     1) / 2 - (vn->vn_nr_item + 1);
		set_parameters(tb, h, -from_l, 0, 1, NULL, -1, -1);
		return CARRY_ON;
	}

	set_parameters(tb, h, 0,
		       -((MAX_NR_KEY(Sh) + 1 - tb->rnum[h] + vn->vn_nr_item +
			  1) / 2 - (vn->vn_nr_item + 1)), 1, NULL, -1, -1);
	return CARRY_ON;
}

static int dc_check_balance_leaf(struct tree_balance *tb, int h)
{
	struct virtual_node *vn = tb->tb_vn;

	int levbytes;
	
	int maxsize, ret;
	struct buffer_head *S0, *F0;
	int lfree, rfree  ;

	S0 = PATH_H_PBUFFER(tb->tb_path, 0);
	F0 = PATH_H_PPARENT(tb->tb_path, 0);

	levbytes = tb->insert_size[h];

	maxsize = MAX_CHILD_SIZE(S0);	

	if (!F0) {		

		RFALSE(-levbytes >= maxsize - B_FREE_SPACE(S0),
		       "vs-8240: attempt to create empty buffer tree");

		set_parameters(tb, h, 0, 0, 1, NULL, -1, -1);
		return NO_BALANCING_NEEDED;
	}

	if ((ret = get_parents(tb, h)) != CARRY_ON)
		return ret;

	
	rfree = get_rfree(tb, h);
	lfree = get_lfree(tb, h);

	create_virtual_node(tb, h);

	
	if (are_leaves_removable(tb, lfree, rfree))
		return CARRY_ON;

	check_left(tb, h, lfree);
	check_right(tb, h, rfree);

	
	if (tb->lnum[0] >= vn->vn_nr_item && tb->lbytes == -1)
		if (is_left_neighbor_in_cache(tb, h) || ((tb->rnum[0] - ((tb->rbytes == -1) ? 0 : 1)) < vn->vn_nr_item) ||	
		    !tb->FR[h]) {

			RFALSE(!tb->FL[h],
			       "vs-8245: dc_check_balance_leaf: FL[h] must exist");

			
			set_parameters(tb, h, -1, 0, 0, NULL, -1, -1);
			return CARRY_ON;
		}

	
	if (tb->rnum[0] >= vn->vn_nr_item && tb->rbytes == -1) {
		set_parameters(tb, h, 0, -1, 0, NULL, -1, -1);
		return CARRY_ON;
	}

	
	if (is_leaf_removable(tb))
		return CARRY_ON;

	
	tb->s0num = vn->vn_nr_item;
	set_parameters(tb, h, 0, 0, 1, NULL, -1, -1);
	return NO_BALANCING_NEEDED;
}

static int dc_check_balance(struct tree_balance *tb, int h)
{
	RFALSE(!(PATH_H_PBUFFER(tb->tb_path, h)),
	       "vs-8250: S is not initialized");

	if (h)
		return dc_check_balance_internal(tb, h);
	else
		return dc_check_balance_leaf(tb, h);
}

static int check_balance(int mode,
			 struct tree_balance *tb,
			 int h,
			 int inum,
			 int pos_in_item,
			 struct item_head *ins_ih, const void *data)
{
	struct virtual_node *vn;

	vn = tb->tb_vn = (struct virtual_node *)(tb->vn_buf);
	vn->vn_free_ptr = (char *)(tb->tb_vn + 1);
	vn->vn_mode = mode;
	vn->vn_affected_item_num = inum;
	vn->vn_pos_in_item = pos_in_item;
	vn->vn_ins_ih = ins_ih;
	vn->vn_data = data;

	RFALSE(mode == M_INSERT && !vn->vn_ins_ih,
	       "vs-8255: ins_ih can not be 0 in insert mode");

	if (tb->insert_size[h] > 0)
		
		return ip_check_balance(tb, h);

	
	return dc_check_balance(tb, h);
}

static int get_direct_parent(struct tree_balance *tb, int h)
{
	struct buffer_head *bh;
	struct treepath *path = tb->tb_path;
	int position,
	    path_offset = PATH_H_PATH_OFFSET(tb->tb_path, h);

	
	if (path_offset <= FIRST_PATH_ELEMENT_OFFSET) {

		RFALSE(path_offset < FIRST_PATH_ELEMENT_OFFSET - 1,
		       "PAP-8260: invalid offset in the path");

		if (PATH_OFFSET_PBUFFER(path, FIRST_PATH_ELEMENT_OFFSET)->
		    b_blocknr == SB_ROOT_BLOCK(tb->tb_sb)) {
			
			PATH_OFFSET_PBUFFER(path, path_offset - 1) = NULL;
			PATH_OFFSET_POSITION(path, path_offset - 1) = 0;
			return CARRY_ON;
		}
		return REPEAT_SEARCH;	
	}

	if (!B_IS_IN_TREE
	    (bh = PATH_OFFSET_PBUFFER(path, path_offset - 1)))
		return REPEAT_SEARCH;	

	if ((position =
	     PATH_OFFSET_POSITION(path,
				  path_offset - 1)) > B_NR_ITEMS(bh))
		return REPEAT_SEARCH;

	if (B_N_CHILD_NUM(bh, position) !=
	    PATH_OFFSET_PBUFFER(path, path_offset)->b_blocknr)
		
		return REPEAT_SEARCH;

	if (buffer_locked(bh)) {
		reiserfs_write_unlock(tb->tb_sb);
		__wait_on_buffer(bh);
		reiserfs_write_lock(tb->tb_sb);
		if (FILESYSTEM_CHANGED_TB(tb))
			return REPEAT_SEARCH;
	}

	return CARRY_ON;	
}

static int get_neighbors(struct tree_balance *tb, int h)
{
	int child_position,
	    path_offset = PATH_H_PATH_OFFSET(tb->tb_path, h + 1);
	unsigned long son_number;
	struct super_block *sb = tb->tb_sb;
	struct buffer_head *bh;

	PROC_INFO_INC(sb, get_neighbors[h]);

	if (tb->lnum[h]) {
		
		PROC_INFO_INC(sb, need_l_neighbor[h]);
		bh = PATH_OFFSET_PBUFFER(tb->tb_path, path_offset);

		RFALSE(bh == tb->FL[h] &&
		       !PATH_OFFSET_POSITION(tb->tb_path, path_offset),
		       "PAP-8270: invalid position in the parent");

		child_position =
		    (bh ==
		     tb->FL[h]) ? tb->lkey[h] : B_NR_ITEMS(tb->
								       FL[h]);
		son_number = B_N_CHILD_NUM(tb->FL[h], child_position);
		reiserfs_write_unlock(sb);
		bh = sb_bread(sb, son_number);
		reiserfs_write_lock(sb);
		if (!bh)
			return IO_ERROR;
		if (FILESYSTEM_CHANGED_TB(tb)) {
			brelse(bh);
			PROC_INFO_INC(sb, get_neighbors_restart[h]);
			return REPEAT_SEARCH;
		}

		RFALSE(!B_IS_IN_TREE(tb->FL[h]) ||
		       child_position > B_NR_ITEMS(tb->FL[h]) ||
		       B_N_CHILD_NUM(tb->FL[h], child_position) !=
		       bh->b_blocknr, "PAP-8275: invalid parent");
		RFALSE(!B_IS_IN_TREE(bh), "PAP-8280: invalid child");
		RFALSE(!h &&
		       B_FREE_SPACE(bh) !=
		       MAX_CHILD_SIZE(bh) -
		       dc_size(B_N_CHILD(tb->FL[0], child_position)),
		       "PAP-8290: invalid child size of left neighbor");

		brelse(tb->L[h]);
		tb->L[h] = bh;
	}

	
	if (tb->rnum[h]) {	
		PROC_INFO_INC(sb, need_r_neighbor[h]);
		bh = PATH_OFFSET_PBUFFER(tb->tb_path, path_offset);

		RFALSE(bh == tb->FR[h] &&
		       PATH_OFFSET_POSITION(tb->tb_path,
					    path_offset) >=
		       B_NR_ITEMS(bh),
		       "PAP-8295: invalid position in the parent");

		child_position =
		    (bh == tb->FR[h]) ? tb->rkey[h] + 1 : 0;
		son_number = B_N_CHILD_NUM(tb->FR[h], child_position);
		reiserfs_write_unlock(sb);
		bh = sb_bread(sb, son_number);
		reiserfs_write_lock(sb);
		if (!bh)
			return IO_ERROR;
		if (FILESYSTEM_CHANGED_TB(tb)) {
			brelse(bh);
			PROC_INFO_INC(sb, get_neighbors_restart[h]);
			return REPEAT_SEARCH;
		}
		brelse(tb->R[h]);
		tb->R[h] = bh;

		RFALSE(!h
		       && B_FREE_SPACE(bh) !=
		       MAX_CHILD_SIZE(bh) -
		       dc_size(B_N_CHILD(tb->FR[0], child_position)),
		       "PAP-8300: invalid child size of right neighbor (%d != %d - %d)",
		       B_FREE_SPACE(bh), MAX_CHILD_SIZE(bh),
		       dc_size(B_N_CHILD(tb->FR[0], child_position)));

	}
	return CARRY_ON;
}

static int get_virtual_node_size(struct super_block *sb, struct buffer_head *bh)
{
	int max_num_of_items;
	int max_num_of_entries;
	unsigned long blocksize = sb->s_blocksize;

#define MIN_NAME_LEN 1

	max_num_of_items = (blocksize - BLKH_SIZE) / (IH_SIZE + MIN_ITEM_LEN);
	max_num_of_entries = (blocksize - BLKH_SIZE - IH_SIZE) /
	    (DEH_SIZE + MIN_NAME_LEN);

	return sizeof(struct virtual_node) +
	    max(max_num_of_items * sizeof(struct virtual_item),
		sizeof(struct virtual_item) + sizeof(struct direntry_uarea) +
		(max_num_of_entries - 1) * sizeof(__u16));
}

static int get_mem_for_virtual_node(struct tree_balance *tb)
{
	int check_fs = 0;
	int size;
	char *buf;

	size = get_virtual_node_size(tb->tb_sb, PATH_PLAST_BUFFER(tb->tb_path));

	if (size > tb->vn_buf_size) {
		
		if (tb->vn_buf) {
			
			kfree(tb->vn_buf);
			
			check_fs = 1;
		}

		
		tb->vn_buf_size = size;

		
		buf = kmalloc(size, GFP_ATOMIC | __GFP_NOWARN);
		if (!buf) {
			free_buffers_in_tb(tb);
			buf = kmalloc(size, GFP_NOFS);
			if (!buf) {
				tb->vn_buf_size = 0;
			}
			tb->vn_buf = buf;
			schedule();
			return REPEAT_SEARCH;
		}

		tb->vn_buf = buf;
	}

	if (check_fs && FILESYSTEM_CHANGED_TB(tb))
		return REPEAT_SEARCH;

	return CARRY_ON;
}

#ifdef CONFIG_REISERFS_CHECK
static void tb_buffer_sanity_check(struct super_block *sb,
				   struct buffer_head *bh,
				   const char *descr, int level)
{
	if (bh) {
		if (atomic_read(&(bh->b_count)) <= 0)

			reiserfs_panic(sb, "jmacd-1", "negative or zero "
				       "reference counter for buffer %s[%d] "
				       "(%b)", descr, level, bh);

		if (!buffer_uptodate(bh))
			reiserfs_panic(sb, "jmacd-2", "buffer is not up "
				       "to date %s[%d] (%b)",
				       descr, level, bh);

		if (!B_IS_IN_TREE(bh))
			reiserfs_panic(sb, "jmacd-3", "buffer is not "
				       "in tree %s[%d] (%b)",
				       descr, level, bh);

		if (bh->b_bdev != sb->s_bdev)
			reiserfs_panic(sb, "jmacd-4", "buffer has wrong "
				       "device %s[%d] (%b)",
				       descr, level, bh);

		if (bh->b_size != sb->s_blocksize)
			reiserfs_panic(sb, "jmacd-5", "buffer has wrong "
				       "blocksize %s[%d] (%b)",
				       descr, level, bh);

		if (bh->b_blocknr > SB_BLOCK_COUNT(sb))
			reiserfs_panic(sb, "jmacd-6", "buffer block "
				       "number too high %s[%d] (%b)",
				       descr, level, bh);
	}
}
#else
static void tb_buffer_sanity_check(struct super_block *sb,
				   struct buffer_head *bh,
				   const char *descr, int level)
{;
}
#endif

static int clear_all_dirty_bits(struct super_block *s, struct buffer_head *bh)
{
	return reiserfs_prepare_for_journal(s, bh, 0);
}

static int wait_tb_buffers_until_unlocked(struct tree_balance *tb)
{
	struct buffer_head *locked;
#ifdef CONFIG_REISERFS_CHECK
	int repeat_counter = 0;
#endif
	int i;

	do {

		locked = NULL;

		for (i = tb->tb_path->path_length;
		     !locked && i > ILLEGAL_PATH_ELEMENT_OFFSET; i--) {
			if (PATH_OFFSET_PBUFFER(tb->tb_path, i)) {
#ifdef CONFIG_REISERFS_CHECK
				if (PATH_PLAST_BUFFER(tb->tb_path) ==
				    PATH_OFFSET_PBUFFER(tb->tb_path, i))
					tb_buffer_sanity_check(tb->tb_sb,
							       PATH_OFFSET_PBUFFER
							       (tb->tb_path,
								i), "S",
							       tb->tb_path->
							       path_length - i);
#endif
				if (!clear_all_dirty_bits(tb->tb_sb,
							  PATH_OFFSET_PBUFFER
							  (tb->tb_path,
							   i))) {
					locked =
					    PATH_OFFSET_PBUFFER(tb->tb_path,
								i);
				}
			}
		}

		for (i = 0; !locked && i < MAX_HEIGHT && tb->insert_size[i];
		     i++) {

			if (tb->lnum[i]) {

				if (tb->L[i]) {
					tb_buffer_sanity_check(tb->tb_sb,
							       tb->L[i],
							       "L", i);
					if (!clear_all_dirty_bits
					    (tb->tb_sb, tb->L[i]))
						locked = tb->L[i];
				}

				if (!locked && tb->FL[i]) {
					tb_buffer_sanity_check(tb->tb_sb,
							       tb->FL[i],
							       "FL", i);
					if (!clear_all_dirty_bits
					    (tb->tb_sb, tb->FL[i]))
						locked = tb->FL[i];
				}

				if (!locked && tb->CFL[i]) {
					tb_buffer_sanity_check(tb->tb_sb,
							       tb->CFL[i],
							       "CFL", i);
					if (!clear_all_dirty_bits
					    (tb->tb_sb, tb->CFL[i]))
						locked = tb->CFL[i];
				}

			}

			if (!locked && (tb->rnum[i])) {

				if (tb->R[i]) {
					tb_buffer_sanity_check(tb->tb_sb,
							       tb->R[i],
							       "R", i);
					if (!clear_all_dirty_bits
					    (tb->tb_sb, tb->R[i]))
						locked = tb->R[i];
				}

				if (!locked && tb->FR[i]) {
					tb_buffer_sanity_check(tb->tb_sb,
							       tb->FR[i],
							       "FR", i);
					if (!clear_all_dirty_bits
					    (tb->tb_sb, tb->FR[i]))
						locked = tb->FR[i];
				}

				if (!locked && tb->CFR[i]) {
					tb_buffer_sanity_check(tb->tb_sb,
							       tb->CFR[i],
							       "CFR", i);
					if (!clear_all_dirty_bits
					    (tb->tb_sb, tb->CFR[i]))
						locked = tb->CFR[i];
				}
			}
		}
		for (i = 0; !locked && i < MAX_FEB_SIZE; i++) {
			if (tb->FEB[i]) {
				if (!clear_all_dirty_bits
				    (tb->tb_sb, tb->FEB[i]))
					locked = tb->FEB[i];
			}
		}

		if (locked) {
#ifdef CONFIG_REISERFS_CHECK
			repeat_counter++;
			if ((repeat_counter % 10000) == 0) {
				reiserfs_warning(tb->tb_sb, "reiserfs-8200",
						 "too many iterations waiting "
						 "for buffer to unlock "
						 "(%b)", locked);

				

				return (FILESYSTEM_CHANGED_TB(tb)) ?
				    REPEAT_SEARCH : CARRY_ON;
			}
#endif
			reiserfs_write_unlock(tb->tb_sb);
			__wait_on_buffer(locked);
			reiserfs_write_lock(tb->tb_sb);
			if (FILESYSTEM_CHANGED_TB(tb))
				return REPEAT_SEARCH;
		}

	} while (locked);

	return CARRY_ON;
}


int fix_nodes(int op_mode, struct tree_balance *tb,
	      struct item_head *ins_ih, const void *data)
{
	int ret, h, item_num = PATH_LAST_POSITION(tb->tb_path);
	int pos_in_item;

	int wait_tb_buffers_run = 0;
	struct buffer_head *tbS0 = PATH_PLAST_BUFFER(tb->tb_path);

	++REISERFS_SB(tb->tb_sb)->s_fix_nodes;

	pos_in_item = tb->tb_path->pos_in_item;

	tb->fs_gen = get_generation(tb->tb_sb);

	reiserfs_prepare_for_journal(tb->tb_sb,
				     SB_BUFFER_WITH_SB(tb->tb_sb), 1);
	journal_mark_dirty(tb->transaction_handle, tb->tb_sb,
			   SB_BUFFER_WITH_SB(tb->tb_sb));
	if (FILESYSTEM_CHANGED_TB(tb))
		return REPEAT_SEARCH;

	
	if (buffer_locked(tbS0)) {
		reiserfs_write_unlock(tb->tb_sb);
		__wait_on_buffer(tbS0);
		reiserfs_write_lock(tb->tb_sb);
		if (FILESYSTEM_CHANGED_TB(tb))
			return REPEAT_SEARCH;
	}
#ifdef CONFIG_REISERFS_CHECK
	if (REISERFS_SB(tb->tb_sb)->cur_tb) {
		print_cur_tb("fix_nodes");
		reiserfs_panic(tb->tb_sb, "PAP-8305",
			       "there is pending do_balance");
	}

	if (!buffer_uptodate(tbS0) || !B_IS_IN_TREE(tbS0))
		reiserfs_panic(tb->tb_sb, "PAP-8320", "S[0] (%b %z) is "
			       "not uptodate at the beginning of fix_nodes "
			       "or not in tree (mode %c)",
			       tbS0, tbS0, op_mode);

	
	switch (op_mode) {
	case M_INSERT:
		if (item_num <= 0 || item_num > B_NR_ITEMS(tbS0))
			reiserfs_panic(tb->tb_sb, "PAP-8330", "Incorrect "
				       "item number %d (in S0 - %d) in case "
				       "of insert", item_num,
				       B_NR_ITEMS(tbS0));
		break;
	case M_PASTE:
	case M_DELETE:
	case M_CUT:
		if (item_num < 0 || item_num >= B_NR_ITEMS(tbS0)) {
			print_block(tbS0, 0, -1, -1);
			reiserfs_panic(tb->tb_sb, "PAP-8335", "Incorrect "
				       "item number(%d); mode = %c "
				       "insert_size = %d",
				       item_num, op_mode,
				       tb->insert_size[0]);
		}
		break;
	default:
		reiserfs_panic(tb->tb_sb, "PAP-8340", "Incorrect mode "
			       "of operation");
	}
#endif

	if (get_mem_for_virtual_node(tb) == REPEAT_SEARCH)
		
		return REPEAT_SEARCH;

	
	for (h = 0; h < MAX_HEIGHT && tb->insert_size[h]; h++) {
		ret = get_direct_parent(tb, h);
		if (ret != CARRY_ON)
			goto repeat;

		ret = check_balance(op_mode, tb, h, item_num,
				    pos_in_item, ins_ih, data);
		if (ret != CARRY_ON) {
			if (ret == NO_BALANCING_NEEDED) {
				
				ret = get_neighbors(tb, h);
				if (ret != CARRY_ON)
					goto repeat;
				if (h != MAX_HEIGHT - 1)
					tb->insert_size[h + 1] = 0;
				
				break;
			}
			goto repeat;
		}

		ret = get_neighbors(tb, h);
		if (ret != CARRY_ON)
			goto repeat;

		ret = get_empty_nodes(tb, h);
		if (ret != CARRY_ON)
			goto repeat;

		if (!PATH_H_PBUFFER(tb->tb_path, h)) {

			RFALSE(tb->blknum[h] != 1,
			       "PAP-8350: creating new empty root");

			if (h < MAX_HEIGHT - 1)
				tb->insert_size[h + 1] = 0;
		} else if (!PATH_H_PBUFFER(tb->tb_path, h + 1)) {
			if (tb->blknum[h] > 1) {

				RFALSE(h == MAX_HEIGHT - 1,
				       "PAP-8355: attempt to create too high of a tree");

				tb->insert_size[h + 1] =
				    (DC_SIZE +
				     KEY_SIZE) * (tb->blknum[h] - 1) +
				    DC_SIZE;
			} else if (h < MAX_HEIGHT - 1)
				tb->insert_size[h + 1] = 0;
		} else
			tb->insert_size[h + 1] =
			    (DC_SIZE + KEY_SIZE) * (tb->blknum[h] - 1);
	}

	ret = wait_tb_buffers_until_unlocked(tb);
	if (ret == CARRY_ON) {
		if (FILESYSTEM_CHANGED_TB(tb)) {
			wait_tb_buffers_run = 1;
			ret = REPEAT_SEARCH;
			goto repeat;
		} else {
			return CARRY_ON;
		}
	} else {
		wait_tb_buffers_run = 1;
		goto repeat;
	}

      repeat:
	
	
	
	
	
	{
		int i;

		
		if (wait_tb_buffers_run) {
			pathrelse_and_restore(tb->tb_sb, tb->tb_path);
		} else {
			pathrelse(tb->tb_path);
		}
		
		for (i = 0; i < MAX_HEIGHT; i++) {
			if (wait_tb_buffers_run) {
				reiserfs_restore_prepared_buffer(tb->tb_sb,
								 tb->L[i]);
				reiserfs_restore_prepared_buffer(tb->tb_sb,
								 tb->R[i]);
				reiserfs_restore_prepared_buffer(tb->tb_sb,
								 tb->FL[i]);
				reiserfs_restore_prepared_buffer(tb->tb_sb,
								 tb->FR[i]);
				reiserfs_restore_prepared_buffer(tb->tb_sb,
								 tb->
								 CFL[i]);
				reiserfs_restore_prepared_buffer(tb->tb_sb,
								 tb->
								 CFR[i]);
			}

			brelse(tb->L[i]);
			brelse(tb->R[i]);
			brelse(tb->FL[i]);
			brelse(tb->FR[i]);
			brelse(tb->CFL[i]);
			brelse(tb->CFR[i]);

			tb->L[i] = NULL;
			tb->R[i] = NULL;
			tb->FL[i] = NULL;
			tb->FR[i] = NULL;
			tb->CFL[i] = NULL;
			tb->CFR[i] = NULL;
		}

		if (wait_tb_buffers_run) {
			for (i = 0; i < MAX_FEB_SIZE; i++) {
				if (tb->FEB[i])
					reiserfs_restore_prepared_buffer
					    (tb->tb_sb, tb->FEB[i]);
			}
		}
		return ret;
	}

}

void unfix_nodes(struct tree_balance *tb)
{
	int i;

	
	pathrelse_and_restore(tb->tb_sb, tb->tb_path);

	
	for (i = 0; i < MAX_HEIGHT; i++) {
		reiserfs_restore_prepared_buffer(tb->tb_sb, tb->L[i]);
		reiserfs_restore_prepared_buffer(tb->tb_sb, tb->R[i]);
		reiserfs_restore_prepared_buffer(tb->tb_sb, tb->FL[i]);
		reiserfs_restore_prepared_buffer(tb->tb_sb, tb->FR[i]);
		reiserfs_restore_prepared_buffer(tb->tb_sb, tb->CFL[i]);
		reiserfs_restore_prepared_buffer(tb->tb_sb, tb->CFR[i]);

		brelse(tb->L[i]);
		brelse(tb->R[i]);
		brelse(tb->FL[i]);
		brelse(tb->FR[i]);
		brelse(tb->CFL[i]);
		brelse(tb->CFR[i]);
	}

	
	for (i = 0; i < MAX_FEB_SIZE; i++) {
		if (tb->FEB[i]) {
			b_blocknr_t blocknr = tb->FEB[i]->b_blocknr;
			brelse(tb->FEB[i]);
			reiserfs_free_block(tb->transaction_handle, NULL,
					    blocknr, 0);
		}
		if (tb->used[i]) {
			
			brelse(tb->used[i]);
		}
	}

	kfree(tb->vn_buf);

}
