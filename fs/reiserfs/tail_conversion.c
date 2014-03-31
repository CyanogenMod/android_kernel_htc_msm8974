/*
 * Copyright 1999 Hans Reiser, see reiserfs/README for licensing and copyright details
 */

#include <linux/time.h>
#include <linux/pagemap.h>
#include <linux/buffer_head.h>
#include "reiserfs.h"


int direct2indirect(struct reiserfs_transaction_handle *th, struct inode *inode,
		    struct treepath *path, struct buffer_head *unbh,
		    loff_t tail_offset)
{
	struct super_block *sb = inode->i_sb;
	struct buffer_head *up_to_date_bh;
	struct item_head *p_le_ih = PATH_PITEM_HEAD(path);
	unsigned long total_tail = 0;
	struct cpu_key end_key;	
	struct item_head ind_ih;	
	int blk_size, retval;	
	unp_t unfm_ptr;		

	BUG_ON(!th->t_trans_id);

	REISERFS_SB(sb)->s_direct2indirect++;

	blk_size = sb->s_blocksize;

	copy_item_head(&ind_ih, p_le_ih);
	set_le_ih_k_offset(&ind_ih, tail_offset);
	set_le_ih_k_type(&ind_ih, TYPE_INDIRECT);

	
	make_cpu_key(&end_key, inode, tail_offset, TYPE_INDIRECT, 4);

	
	if (search_for_position_by_key(sb, &end_key, path) == POSITION_FOUND) {
		reiserfs_error(sb, "PAP-14030",
			       "pasted or inserted byte exists in "
			       "the tree %K. Use fsck to repair.", &end_key);
		pathrelse(path);
		return -EIO;
	}

	p_le_ih = PATH_PITEM_HEAD(path);

	unfm_ptr = cpu_to_le32(unbh->b_blocknr);

	if (is_statdata_le_ih(p_le_ih)) {
		
		set_ih_free_space(&ind_ih, 0);	
		put_ih_item_len(&ind_ih, UNFM_P_SIZE);
		PATH_LAST_POSITION(path)++;
		retval =
		    reiserfs_insert_item(th, path, &end_key, &ind_ih, inode,
					 (char *)&unfm_ptr);
	} else {
		
		retval = reiserfs_paste_into_item(th, path, &end_key, inode,
						    (char *)&unfm_ptr,
						    UNFM_P_SIZE);
	}
	if (retval) {
		return retval;
	}
	
	

	
	make_cpu_key(&end_key, inode, max_reiserfs_offset(inode), TYPE_DIRECT,
		     4);

	while (1) {
		int tail_size;

		if (search_for_position_by_key(sb, &end_key, path) ==
		    POSITION_FOUND)
			reiserfs_panic(sb, "PAP-14050",
				       "direct item (%K) not found", &end_key);
		p_le_ih = PATH_PITEM_HEAD(path);
		RFALSE(!is_direct_le_ih(p_le_ih),
		       "vs-14055: direct item expected(%K), found %h",
		       &end_key, p_le_ih);
		tail_size = (le_ih_k_offset(p_le_ih) & (blk_size - 1))
		    + ih_item_len(p_le_ih) - 1;

		if (!unbh->b_page || buffer_uptodate(unbh)
		    || PageUptodate(unbh->b_page)) {
			up_to_date_bh = NULL;
		} else {
			up_to_date_bh = unbh;
		}
		retval = reiserfs_delete_item(th, path, &end_key, inode,
						up_to_date_bh);

		total_tail += retval;
		if (tail_size == retval)
			
			break;

	}
	if (up_to_date_bh) {
		unsigned pgoff =
		    (tail_offset + total_tail - 1) & (PAGE_CACHE_SIZE - 1);
		char *kaddr = kmap_atomic(up_to_date_bh->b_page);
		memset(kaddr + pgoff, 0, blk_size - total_tail);
		kunmap_atomic(kaddr);
	}

	REISERFS_I(inode)->i_first_direct_byte = U32_MAX;

	return 0;
}

void reiserfs_unmap_buffer(struct buffer_head *bh)
{
	lock_buffer(bh);
	if (buffer_journaled(bh) || buffer_journal_dirty(bh)) {
		BUG();
	}
	clear_buffer_dirty(bh);
	if ((!list_empty(&bh->b_assoc_buffers) || bh->b_private) && bh->b_page) {
		struct inode *inode = bh->b_page->mapping->host;
		struct reiserfs_journal *j = SB_JOURNAL(inode->i_sb);
		spin_lock(&j->j_dirty_buffers_lock);
		list_del_init(&bh->b_assoc_buffers);
		reiserfs_free_jh(bh);
		spin_unlock(&j->j_dirty_buffers_lock);
	}
	clear_buffer_mapped(bh);
	clear_buffer_req(bh);
	clear_buffer_new(bh);
	bh->b_bdev = NULL;
	unlock_buffer(bh);
}

int indirect2direct(struct reiserfs_transaction_handle *th,
		    struct inode *inode, struct page *page,
		    struct treepath *path,	
		    const struct cpu_key *item_key,	
		    loff_t n_new_file_size,	
		    char *mode)
{
	struct super_block *sb = inode->i_sb;
	struct item_head s_ih;
	unsigned long block_size = sb->s_blocksize;
	char *tail;
	int tail_len, round_tail_len;
	loff_t pos, pos1;	
	struct cpu_key key;

	BUG_ON(!th->t_trans_id);

	REISERFS_SB(sb)->s_indirect2direct++;

	*mode = M_SKIP_BALANCING;

	
	copy_item_head(&s_ih, PATH_PITEM_HEAD(path));

	tail_len = (n_new_file_size & (block_size - 1));
	if (get_inode_sd_version(inode) == STAT_DATA_V2)
		round_tail_len = ROUND_UP(tail_len);
	else
		round_tail_len = tail_len;

	pos =
	    le_ih_k_offset(&s_ih) - 1 + (ih_item_len(&s_ih) / UNFM_P_SIZE -
					 1) * sb->s_blocksize;
	pos1 = pos;

	
	
	

	tail = (char *)kmap(page);	

	if (path_changed(&s_ih, path)) {
		
		if (search_for_position_by_key(sb, item_key, path)
		    == POSITION_NOT_FOUND)
			reiserfs_panic(sb, "PAP-5520",
				       "item to be converted %K does not exist",
				       item_key);
		copy_item_head(&s_ih, PATH_PITEM_HEAD(path));
#ifdef CONFIG_REISERFS_CHECK
		pos = le_ih_k_offset(&s_ih) - 1 +
		    (ih_item_len(&s_ih) / UNFM_P_SIZE -
		     1) * sb->s_blocksize;
		if (pos != pos1)
			reiserfs_panic(sb, "vs-5530", "tail position "
				       "changed while we were reading it");
#endif
	}

	
	make_le_item_head(&s_ih, NULL, get_inode_item_key_version(inode),
			  pos1 + 1, TYPE_DIRECT, round_tail_len,
			  0xffff  );

	tail = tail + (pos & (PAGE_CACHE_SIZE - 1));

	PATH_LAST_POSITION(path)++;

	key = *item_key;
	set_cpu_key_k_type(&key, TYPE_DIRECT);
	key.key_length = 4;
	
	if (reiserfs_insert_item(th, path, &key, &s_ih, inode,
				 tail ? tail : NULL) < 0) {
		kunmap(page);
		return block_size - round_tail_len;
	}
	kunmap(page);

	
	reiserfs_update_sd(th, inode);

	
	
	

	*mode = M_CUT;

	
	
	REISERFS_I(inode)->i_first_direct_byte = pos1 + 1;

	return block_size - round_tail_len;
}
