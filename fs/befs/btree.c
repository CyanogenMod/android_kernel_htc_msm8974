/*
 * linux/fs/befs/btree.c
 *
 * Copyright (C) 2001-2002 Will Dyson <will_dyson@pobox.com>
 *
 * Licensed under the GNU GPL. See the file COPYING for details.
 *
 * 2002-02-05: Sergey S. Kostyliov added binary search within
 * 		btree nodes.
 *
 * Many thanks to:
 *
 * Dominic Giampaolo, author of "Practical File System
 * Design with the Be File System", for such a helpful book.
 * 
 * Marcus J. Ranum, author of the b+tree package in 
 * comp.sources.misc volume 10. This code is not copied from that
 * work, but it is partially based on it.
 *
 * Makoto Kato, author of the original BeFS for linux filesystem
 * driver.
 */

#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/buffer_head.h>

#include "befs.h"
#include "btree.h"
#include "datastream.h"





typedef struct {
	befs_host_btree_nodehead head;	
	struct buffer_head *bh;
	befs_btree_nodehead *od_node;	
} befs_btree_node;

static const befs_off_t befs_bt_inval = 0xffffffffffffffffULL;

static int befs_btree_seekleaf(struct super_block *sb, befs_data_stream * ds,
			       befs_btree_super * bt_super,
			       befs_btree_node * this_node,
			       befs_off_t * node_off);

static int befs_bt_read_super(struct super_block *sb, befs_data_stream * ds,
			      befs_btree_super * sup);

static int befs_bt_read_node(struct super_block *sb, befs_data_stream * ds,
			     befs_btree_node * node, befs_off_t node_off);

static int befs_leafnode(befs_btree_node * node);

static fs16 *befs_bt_keylen_index(befs_btree_node * node);

static fs64 *befs_bt_valarray(befs_btree_node * node);

static char *befs_bt_keydata(befs_btree_node * node);

static int befs_find_key(struct super_block *sb, befs_btree_node * node,
			 const char *findkey, befs_off_t * value);

static char *befs_bt_get_key(struct super_block *sb, befs_btree_node * node,
			     int index, u16 * keylen);

static int befs_compare_strings(const void *key1, int keylen1,
				const void *key2, int keylen2);

static int
befs_bt_read_super(struct super_block *sb, befs_data_stream * ds,
		   befs_btree_super * sup)
{
	struct buffer_head *bh = NULL;
	befs_disk_btree_super *od_sup = NULL;

	befs_debug(sb, "---> befs_btree_read_super()");

	bh = befs_read_datastream(sb, ds, 0, NULL);

	if (!bh) {
		befs_error(sb, "Couldn't read index header.");
		goto error;
	}
	od_sup = (befs_disk_btree_super *) bh->b_data;
	befs_dump_index_entry(sb, od_sup);

	sup->magic = fs32_to_cpu(sb, od_sup->magic);
	sup->node_size = fs32_to_cpu(sb, od_sup->node_size);
	sup->max_depth = fs32_to_cpu(sb, od_sup->max_depth);
	sup->data_type = fs32_to_cpu(sb, od_sup->data_type);
	sup->root_node_ptr = fs64_to_cpu(sb, od_sup->root_node_ptr);
	sup->free_node_ptr = fs64_to_cpu(sb, od_sup->free_node_ptr);
	sup->max_size = fs64_to_cpu(sb, od_sup->max_size);

	brelse(bh);
	if (sup->magic != BEFS_BTREE_MAGIC) {
		befs_error(sb, "Index header has bad magic.");
		goto error;
	}

	befs_debug(sb, "<--- befs_btree_read_super()");
	return BEFS_OK;

      error:
	befs_debug(sb, "<--- befs_btree_read_super() ERROR");
	return BEFS_ERR;
}


static int
befs_bt_read_node(struct super_block *sb, befs_data_stream * ds,
		  befs_btree_node * node, befs_off_t node_off)
{
	uint off = 0;

	befs_debug(sb, "---> befs_bt_read_node()");

	if (node->bh)
		brelse(node->bh);

	node->bh = befs_read_datastream(sb, ds, node_off, &off);
	if (!node->bh) {
		befs_error(sb, "befs_bt_read_node() failed to read "
			   "node at %Lu", node_off);
		befs_debug(sb, "<--- befs_bt_read_node() ERROR");

		return BEFS_ERR;
	}
	node->od_node =
	    (befs_btree_nodehead *) ((void *) node->bh->b_data + off);

	befs_dump_index_node(sb, node->od_node);

	node->head.left = fs64_to_cpu(sb, node->od_node->left);
	node->head.right = fs64_to_cpu(sb, node->od_node->right);
	node->head.overflow = fs64_to_cpu(sb, node->od_node->overflow);
	node->head.all_key_count =
	    fs16_to_cpu(sb, node->od_node->all_key_count);
	node->head.all_key_length =
	    fs16_to_cpu(sb, node->od_node->all_key_length);

	befs_debug(sb, "<--- befs_btree_read_node()");
	return BEFS_OK;
}

int
befs_btree_find(struct super_block *sb, befs_data_stream * ds,
		const char *key, befs_off_t * value)
{
	befs_btree_node *this_node = NULL;
	befs_btree_super bt_super;
	befs_off_t node_off;
	int res;

	befs_debug(sb, "---> befs_btree_find() Key: %s", key);

	if (befs_bt_read_super(sb, ds, &bt_super) != BEFS_OK) {
		befs_error(sb,
			   "befs_btree_find() failed to read index superblock");
		goto error;
	}

	this_node = kmalloc(sizeof (befs_btree_node),
						GFP_NOFS);
	if (!this_node) {
		befs_error(sb, "befs_btree_find() failed to allocate %u "
			   "bytes of memory", sizeof (befs_btree_node));
		goto error;
	}

	this_node->bh = NULL;

	
	node_off = bt_super.root_node_ptr;
	if (befs_bt_read_node(sb, ds, this_node, node_off) != BEFS_OK) {
		befs_error(sb, "befs_btree_find() failed to read "
			   "node at %Lu", node_off);
		goto error_alloc;
	}

	while (!befs_leafnode(this_node)) {
		res = befs_find_key(sb, this_node, key, &node_off);
		if (res == BEFS_BT_NOT_FOUND)
			node_off = this_node->head.overflow;
		
		if (befs_bt_read_node(sb, ds, this_node, node_off) != BEFS_OK) {
			befs_error(sb, "befs_btree_find() failed to read "
				   "node at %Lu", node_off);
			goto error_alloc;
		}
	}

	

	res = befs_find_key(sb, this_node, key, value);

	brelse(this_node->bh);
	kfree(this_node);

	if (res != BEFS_BT_MATCH) {
		befs_debug(sb, "<--- befs_btree_find() Key %s not found", key);
		*value = 0;
		return BEFS_BT_NOT_FOUND;
	}
	befs_debug(sb, "<--- befs_btree_find() Found key %s, value %Lu",
		   key, *value);
	return BEFS_OK;

      error_alloc:
	kfree(this_node);
      error:
	*value = 0;
	befs_debug(sb, "<--- befs_btree_find() ERROR");
	return BEFS_ERR;
}

static int
befs_find_key(struct super_block *sb, befs_btree_node * node,
	      const char *findkey, befs_off_t * value)
{
	int first, last, mid;
	int eq;
	u16 keylen;
	int findkey_len;
	char *thiskey;
	fs64 *valarray;

	befs_debug(sb, "---> befs_find_key() %s", findkey);

	*value = 0;

	findkey_len = strlen(findkey);

	
	last = node->head.all_key_count - 1;
	thiskey = befs_bt_get_key(sb, node, last, &keylen);

	eq = befs_compare_strings(thiskey, keylen, findkey, findkey_len);
	if (eq < 0) {
		befs_debug(sb, "<--- befs_find_key() %s not found", findkey);
		return BEFS_BT_NOT_FOUND;
	}

	valarray = befs_bt_valarray(node);

	
	first = 0;
	mid = 0;
	while (last >= first) {
		mid = (last + first) / 2;
		befs_debug(sb, "first: %d, last: %d, mid: %d", first, last,
			   mid);
		thiskey = befs_bt_get_key(sb, node, mid, &keylen);
		eq = befs_compare_strings(thiskey, keylen, findkey,
					  findkey_len);

		if (eq == 0) {
			befs_debug(sb, "<--- befs_find_key() found %s at %d",
				   thiskey, mid);

			*value = fs64_to_cpu(sb, valarray[mid]);
			return BEFS_BT_MATCH;
		}
		if (eq > 0)
			last = mid - 1;
		else
			first = mid + 1;
	}
	if (eq < 0)
		*value = fs64_to_cpu(sb, valarray[mid + 1]);
	else
		*value = fs64_to_cpu(sb, valarray[mid]);
	befs_debug(sb, "<--- befs_find_key() found %s at %d", thiskey, mid);
	return BEFS_BT_PARMATCH;
}

int
befs_btree_read(struct super_block *sb, befs_data_stream * ds,
		loff_t key_no, size_t bufsize, char *keybuf, size_t * keysize,
		befs_off_t * value)
{
	befs_btree_node *this_node;
	befs_btree_super bt_super;
	befs_off_t node_off = 0;
	int cur_key;
	fs64 *valarray;
	char *keystart;
	u16 keylen;
	int res;

	uint key_sum = 0;

	befs_debug(sb, "---> befs_btree_read()");

	if (befs_bt_read_super(sb, ds, &bt_super) != BEFS_OK) {
		befs_error(sb,
			   "befs_btree_read() failed to read index superblock");
		goto error;
	}

	if ((this_node = (befs_btree_node *)
	     kmalloc(sizeof (befs_btree_node), GFP_NOFS)) == NULL) {
		befs_error(sb, "befs_btree_read() failed to allocate %u "
			   "bytes of memory", sizeof (befs_btree_node));
		goto error;
	}

	node_off = bt_super.root_node_ptr;
	this_node->bh = NULL;

	
	res = befs_btree_seekleaf(sb, ds, &bt_super, this_node, &node_off);
	if (res == BEFS_BT_EMPTY) {
		brelse(this_node->bh);
		kfree(this_node);
		*value = 0;
		*keysize = 0;
		befs_debug(sb, "<--- befs_btree_read() Tree is EMPTY");
		return BEFS_BT_EMPTY;
	} else if (res == BEFS_ERR) {
		goto error_alloc;
	}

	

	while (key_sum + this_node->head.all_key_count <= key_no) {

		
		if (this_node->head.right == befs_bt_inval) {
			*keysize = 0;
			*value = 0;
			befs_debug(sb,
				   "<--- befs_btree_read() END of keys at %Lu",
				   key_sum + this_node->head.all_key_count);
			brelse(this_node->bh);
			kfree(this_node);
			return BEFS_BT_END;
		}

		key_sum += this_node->head.all_key_count;
		node_off = this_node->head.right;

		if (befs_bt_read_node(sb, ds, this_node, node_off) != BEFS_OK) {
			befs_error(sb, "befs_btree_read() failed to read "
				   "node at %Lu", node_off);
			goto error_alloc;
		}
	}

	
	cur_key = key_no - key_sum;

	
	valarray = befs_bt_valarray(this_node);

	keystart = befs_bt_get_key(sb, this_node, cur_key, &keylen);

	befs_debug(sb, "Read [%Lu,%d]: keysize %d", node_off, cur_key, keylen);

	if (bufsize < keylen + 1) {
		befs_error(sb, "befs_btree_read() keybuf too small (%u) "
			   "for key of size %d", bufsize, keylen);
		brelse(this_node->bh);
		goto error_alloc;
	};

	strncpy(keybuf, keystart, keylen);
	*value = fs64_to_cpu(sb, valarray[cur_key]);
	*keysize = keylen;
	keybuf[keylen] = '\0';

	befs_debug(sb, "Read [%Lu,%d]: Key \"%.*s\", Value %Lu", node_off,
		   cur_key, keylen, keybuf, *value);

	brelse(this_node->bh);
	kfree(this_node);

	befs_debug(sb, "<--- befs_btree_read()");

	return BEFS_OK;

      error_alloc:
	kfree(this_node);

      error:
	*keysize = 0;
	*value = 0;
	befs_debug(sb, "<--- befs_btree_read() ERROR");
	return BEFS_ERR;
}

static int
befs_btree_seekleaf(struct super_block *sb, befs_data_stream * ds,
		    befs_btree_super * bt_super, befs_btree_node * this_node,
		    befs_off_t * node_off)
{

	befs_debug(sb, "---> befs_btree_seekleaf()");

	if (befs_bt_read_node(sb, ds, this_node, *node_off) != BEFS_OK) {
		befs_error(sb, "befs_btree_seekleaf() failed to read "
			   "node at %Lu", *node_off);
		goto error;
	}
	befs_debug(sb, "Seekleaf to root node %Lu", *node_off);

	if (this_node->head.all_key_count == 0 && befs_leafnode(this_node)) {
		befs_debug(sb, "<--- befs_btree_seekleaf() Tree is EMPTY");
		return BEFS_BT_EMPTY;
	}

	while (!befs_leafnode(this_node)) {

		if (this_node->head.all_key_count == 0) {
			befs_debug(sb, "befs_btree_seekleaf() encountered "
				   "an empty interior node: %Lu. Using Overflow "
				   "node: %Lu", *node_off,
				   this_node->head.overflow);
			*node_off = this_node->head.overflow;
		} else {
			fs64 *valarray = befs_bt_valarray(this_node);
			*node_off = fs64_to_cpu(sb, valarray[0]);
		}
		if (befs_bt_read_node(sb, ds, this_node, *node_off) != BEFS_OK) {
			befs_error(sb, "befs_btree_seekleaf() failed to read "
				   "node at %Lu", *node_off);
			goto error;
		}

		befs_debug(sb, "Seekleaf to child node %Lu", *node_off);
	}
	befs_debug(sb, "Node %Lu is a leaf node", *node_off);

	return BEFS_OK;

      error:
	befs_debug(sb, "<--- befs_btree_seekleaf() ERROR");
	return BEFS_ERR;
}

static int
befs_leafnode(befs_btree_node * node)
{
	
	if (node->head.overflow == befs_bt_inval)
		return 1;
	else
		return 0;
}

static fs16 *
befs_bt_keylen_index(befs_btree_node * node)
{
	const int keylen_align = 8;
	unsigned long int off =
	    (sizeof (befs_btree_nodehead) + node->head.all_key_length);
	ulong tmp = off % keylen_align;

	if (tmp)
		off += keylen_align - tmp;

	return (fs16 *) ((void *) node->od_node + off);
}

static fs64 *
befs_bt_valarray(befs_btree_node * node)
{
	void *keylen_index_start = (void *) befs_bt_keylen_index(node);
	size_t keylen_index_size = node->head.all_key_count * sizeof (fs16);

	return (fs64 *) (keylen_index_start + keylen_index_size);
}

static char *
befs_bt_keydata(befs_btree_node * node)
{
	return (char *) ((void *) node->od_node + sizeof (befs_btree_nodehead));
}

static char *
befs_bt_get_key(struct super_block *sb, befs_btree_node * node,
		int index, u16 * keylen)
{
	int prev_key_end;
	char *keystart;
	fs16 *keylen_index;

	if (index < 0 || index > node->head.all_key_count) {
		*keylen = 0;
		return NULL;
	}

	keystart = befs_bt_keydata(node);
	keylen_index = befs_bt_keylen_index(node);

	if (index == 0)
		prev_key_end = 0;
	else
		prev_key_end = fs16_to_cpu(sb, keylen_index[index - 1]);

	*keylen = fs16_to_cpu(sb, keylen_index[index]) - prev_key_end;

	return keystart + prev_key_end;
}

static int
befs_compare_strings(const void *key1, int keylen1,
		     const void *key2, int keylen2)
{
	int len = min_t(int, keylen1, keylen2);
	int result = strncmp(key1, key2, len);
	if (result == 0)
		result = keylen1 - keylen2;
	return result;
}

#if 0
static int
btree_compare_int32(cont void *key1, int keylen1, const void *key2, int keylen2)
{
	return *(int32_t *) key1 - *(int32_t *) key2;
}

static int
btree_compare_uint32(cont void *key1, int keylen1,
		     const void *key2, int keylen2)
{
	if (*(u_int32_t *) key1 == *(u_int32_t *) key2)
		return 0;
	else if (*(u_int32_t *) key1 > *(u_int32_t *) key2)
		return 1;

	return -1;
}
static int
btree_compare_int64(cont void *key1, int keylen1, const void *key2, int keylen2)
{
	if (*(int64_t *) key1 == *(int64_t *) key2)
		return 0;
	else if (*(int64_t *) key1 > *(int64_t *) key2)
		return 1;

	return -1;
}

static int
btree_compare_uint64(cont void *key1, int keylen1,
		     const void *key2, int keylen2)
{
	if (*(u_int64_t *) key1 == *(u_int64_t *) key2)
		return 0;
	else if (*(u_int64_t *) key1 > *(u_int64_t *) key2)
		return 1;

	return -1;
}

static int
btree_compare_float(cont void *key1, int keylen1, const void *key2, int keylen2)
{
	float result = *(float *) key1 - *(float *) key2;
	if (result == 0.0f)
		return 0;

	return (result < 0.0f) ? -1 : 1;
}

static int
btree_compare_double(cont void *key1, int keylen1,
		     const void *key2, int keylen2)
{
	double result = *(double *) key1 - *(double *) key2;
	if (result == 0.0)
		return 0;

	return (result < 0.0) ? -1 : 1;
}
#endif				
