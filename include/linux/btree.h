#ifndef BTREE_H
#define BTREE_H

#include <linux/kernel.h>
#include <linux/mempool.h>


struct btree_head {
	unsigned long *node;
	mempool_t *mempool;
	int height;
};

struct btree_geo;

void *btree_alloc(gfp_t gfp_mask, void *pool_data);

void btree_free(void *element, void *pool_data);

void btree_init_mempool(struct btree_head *head, mempool_t *mempool);

int __must_check btree_init(struct btree_head *head);

void btree_destroy(struct btree_head *head);

void *btree_lookup(struct btree_head *head, struct btree_geo *geo,
		   unsigned long *key);

int __must_check btree_insert(struct btree_head *head, struct btree_geo *geo,
			      unsigned long *key, void *val, gfp_t gfp);
int btree_update(struct btree_head *head, struct btree_geo *geo,
		 unsigned long *key, void *val);
void *btree_remove(struct btree_head *head, struct btree_geo *geo,
		   unsigned long *key);

int btree_merge(struct btree_head *target, struct btree_head *victim,
		struct btree_geo *geo, gfp_t gfp);

void *btree_last(struct btree_head *head, struct btree_geo *geo,
		 unsigned long *key);

void *btree_get_prev(struct btree_head *head, struct btree_geo *geo,
		     unsigned long *key);


size_t btree_visitor(struct btree_head *head, struct btree_geo *geo,
		     unsigned long opaque,
		     void (*func)(void *elem, unsigned long opaque,
				  unsigned long *key, size_t index,
				  void *func2),
		     void *func2);

size_t btree_grim_visitor(struct btree_head *head, struct btree_geo *geo,
			  unsigned long opaque,
			  void (*func)(void *elem, unsigned long opaque,
				       unsigned long *key,
				       size_t index, void *func2),
			  void *func2);


#include <linux/btree-128.h>

extern struct btree_geo btree_geo32;
#define BTREE_TYPE_SUFFIX l
#define BTREE_TYPE_BITS BITS_PER_LONG
#define BTREE_TYPE_GEO &btree_geo32
#define BTREE_KEYTYPE unsigned long
#include <linux/btree-type.h>

#define btree_for_each_safel(head, key, val)	\
	for (val = btree_lastl(head, &key);	\
	     val;				\
	     val = btree_get_prevl(head, &key))

#define BTREE_TYPE_SUFFIX 32
#define BTREE_TYPE_BITS 32
#define BTREE_TYPE_GEO &btree_geo32
#define BTREE_KEYTYPE u32
#include <linux/btree-type.h>

#define btree_for_each_safe32(head, key, val)	\
	for (val = btree_last32(head, &key);	\
	     val;				\
	     val = btree_get_prev32(head, &key))

extern struct btree_geo btree_geo64;
#define BTREE_TYPE_SUFFIX 64
#define BTREE_TYPE_BITS 64
#define BTREE_TYPE_GEO &btree_geo64
#define BTREE_KEYTYPE u64
#include <linux/btree-type.h>

#define btree_for_each_safe64(head, key, val)	\
	for (val = btree_last64(head, &key);	\
	     val;				\
	     val = btree_get_prev64(head, &key))

#endif
